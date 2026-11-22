#include "LearnedDynamics.h"
#include "AdamOptimizer.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Підпис формату файлу моделі динаміки
static const char DYNAMICS_MAGIC[4] = { 'E', 'D', 'M', '1' };
// Версія формату файлу моделі динаміки: 2 додала модель зіткнень
static const int DYNAMICS_VERSION = 2;
// Найменше відхилення, на яке ділиться величина під час нормування
static const float MIN_SCALE = 1e-4f;

const char* LearnedDynamics::FILE_EXTENSION = ".dyn";
const float LearnedDynamics::COLLISION_RANGE = 20.0f;

// Розміри шарів моделі зіткнень: огляд, стан і дія, два приховані шари, логарифм шансів зіткнення
static std::vector<int> collisionLayers()
{
	return { LearnedDynamics::COLLISION_RAYS + LearnedDynamics::STATE_SIZE + LearnedDynamics::ACTION_SIZE, 64, 64, 1 };
}

// Розміри шарів мережі: стан і дія, два приховані шари по 128 нейронів, приріст стану
std::vector<int> LearnedDynamics::defaultLayers()
{
	return { STATE_SIZE + ACTION_SIZE, 128, 128, STATE_SIZE };
}

// Створює нову модель з випадковими вагами й тотожним нормуванням
void LearnedDynamics::create(std::mt19937& random)
{
	mNetwork.build(defaultLayers(), true);
	mNetwork.randomize(random);

	for (int i = 0; i < 6; i++) { mInputMean[i] = 0.0f; mInputScale[i] = 1.0f; }
	for (int i = 0; i < 4; i++) { mOutputMean[i] = 0.0f; mOutputScale[i] = 1.0f; }

	mNormalized = false;
	mTrainedSamples = 0;
	mHorizonError = 0.0f;
	mHorizon = 0;
}

// Перевіряє, чи модель має мережу потрібного розміру
bool LearnedDynamics::isReady() const
{
	return mNetwork.getLayers() == defaultLayers();
}

// Обчислює середні й відхилення входів і приростів за вибіркою переходів
void LearnedDynamics::computeNormalization(const std::vector<DynamicsTransition>& data)
{
	if (data.empty()) return;

	double inSum[6] = {}, inSquares[6] = {}, outSum[4] = {}, outSquares[4] = {};

	for (const DynamicsTransition& t : data)
	{
		for (int i = 0; i < 4; i++) { inSum[i] += t.state[i]; inSquares[i] += (double)t.state[i] * t.state[i]; }
		for (int i = 0; i < 2; i++) { inSum[4 + i] += t.action[i]; inSquares[4 + i] += (double)t.action[i] * t.action[i]; }

		for (int i = 0; i < 4; i++)
		{
			double delta = (double)t.next[i] - t.state[i];
			outSum[i] += delta;
			outSquares[i] += delta * delta;
		}
	}

	double n = (double)data.size();

	for (int i = 0; i < 6; i++)
	{
		double mean = inSum[i] / n;
		mInputMean[i] = (float)mean;
		mInputScale[i] = (float)std::max((double)MIN_SCALE, std::sqrt(std::max(0.0, inSquares[i] / n - mean * mean)));
	}

	for (int i = 0; i < 4; i++)
	{
		double mean = outSum[i] / n;
		mOutputMean[i] = (float)mean;
		mOutputScale[i] = (float)std::max((double)MIN_SCALE, std::sqrt(std::max(0.0, outSquares[i] / n - mean * mean)));
	}

	mNormalized = true;
}

// Перевіряє, чи нормування вже обчислено
bool LearnedDynamics::hasNormalization() const
{
	return mNormalized;
}

// Нормує вхід мережі: стан і дію
void LearnedDynamics::normalizeInput(const float* state, const float* action, float* input) const
{
	for (int i = 0; i < 4; i++) input[i] = (state[i] - mInputMean[i]) / mInputScale[i];
	for (int i = 0; i < 2; i++) input[4 + i] = (action[i] - mInputMean[4 + i]) / mInputScale[4 + i];
}

// Нормує ціль навчання: приріст стану
void LearnedDynamics::normalizeTarget(const float* state, const float* next, float* target) const
{
	for (int i = 0; i < 4; i++) target[i] = (next[i] - state[i] - mOutputMean[i]) / mOutputScale[i];
}

// Передбачає наступний стан за поточним станом і дією
void LearnedDynamics::predict(const float* state, const float* action, float* next) const
{
	float input[6];
	float output[4];

	normalizeInput(state, action, input);
	mNetwork.forward(input, output);

	for (int i = 0; i < 4; i++) next[i] = state[i] + output[i] * mOutputScale[i] + mOutputMean[i];
}

// Передбачає наступні стани для count пар стан-дія підряд
void LearnedDynamics::predictBatch(const float* states, const float* actions, int count, float* next) const
{
	mBatchInput.resize((size_t)count * 6);
	mBatchOutput.resize((size_t)count * 4);

	for (int s = 0; s < count; s++) normalizeInput(states + s * 4, actions + s * 2, mBatchInput.data() + s * 6);

	mNetwork.forwardBatch(mBatchInput.data(), count, mBatchOutput.data());

	for (int s = 0; s < count; s++)
	{
		for (int i = 0; i < 4; i++)
		{
			next[s * 4 + i] = states[s * 4 + i] + mBatchOutput[s * 4 + i] * mOutputScale[i] + mOutputMean[i];
		}
	}
}

// Переносить положення й курс у площині на крок за передбаченим станом; курс 0 означає напрям уздовж осі Z
void LearnedDynamics::integratePose(float& x, float& z, float& heading, const float* state, float dt)
{
	heading += state[2] * dt;

	// Вперед - (sin, cos), праворуч - (cos, -sin): так рушій розташовує осі корпусу за курсом
	float s = std::sin(heading);
	float c = std::cos(heading);

	x += (s * state[0] + c * state[1]) * dt;
	z += (c * state[0] - s * state[1]) * dt;
}

// Робить крок оптимізатора на пакеті переходів і повертає середню нормовану похибку пакета
float LearnedDynamics::trainBatch(const DynamicsTransition* const* batch, int count, AdamOptimizer& optimizer)
{
	if (count <= 0) return 0.0f;

	mGradients.assign(mNetwork.getParameterCount(), 0.0f);

	float input[6];
	float target[4];
	float loss = 0.0f;

	for (int k = 0; k < count; k++)
	{
		normalizeInput(batch[k]->state, batch[k]->action, input);
		normalizeTarget(batch[k]->state, batch[k]->next, target);
		loss += mNetwork.accumulateGradient(input, target, mGradients.data());
	}

	// Градієнт усереднюється за пакетом, тож темп навчання не залежить від його розміру
	float scale = 1.0f / (float)count;

	for (float& g : mGradients) g *= scale;

	optimizer.step(mNetwork.getParameterData(), mGradients.data());

	mTrainedSamples += count;

	return loss / (float)count;
}

// Повертає середню нормовану похибку одного кроку на переходах
float LearnedDynamics::evaluate(const DynamicsTransition* const* batch, int count) const
{
	if (count <= 0) return 0.0f;

	float input[6];
	float target[4];
	float output[4];
	float loss = 0.0f;

	for (int k = 0; k < count; k++)
	{
		normalizeInput(batch[k]->state, batch[k]->action, input);
		normalizeTarget(batch[k]->state, batch[k]->next, target);
		mNetwork.forward(input, output);

		for (int i = 0; i < 4; i++) loss += (output[i] - target[i]) * (output[i] - target[i]) / 4.0f;
	}

	return loss / (float)count;
}

// Повертає мережу моделі
NeuralNetwork& LearnedDynamics::getNetwork()
{
	return mNetwork;
}

// Повертає кількість переходів, на яких модель навчалася
long long LearnedDynamics::getTrainedSamples() const
{
	return mTrainedSamples;
}

// Повертає похибку положення на горизонті, виміряну під час останньої перевірки
float LearnedDynamics::getHorizonError() const
{
	return mHorizonError;
}

// Запам'ятовує результати перевірки, щоб вони потрапили у файл
void LearnedDynamics::setStatistics(long long trainedSamples, float horizonError, int horizon)
{
	mTrainedSamples = trainedSamples;
	mHorizonError = horizonError;
	mHorizon = horizon;
}

// Створює нову модель зіткнень з випадковими вагами
void LearnedDynamics::createCollisionModel(std::mt19937& random)
{
	mCollisionNetwork.build(collisionLayers(), true);
	mCollisionNetwork.randomize(random);
}

// Перевіряє, чи є модель зіткнень
bool LearnedDynamics::hasCollisionModel() const
{
	return mCollisionNetwork.getLayers() == collisionLayers();
}

// Повертає мережу моделі зіткнень
NeuralNetwork& LearnedDynamics::getCollisionNetwork()
{
	return mCollisionNetwork;
}

// Складає нормований вхід моделі зіткнень
void LearnedDynamics::collisionInput(const float* scan, const float* state, const float* action, float* input) const
{
	// Відстані вже нормовані дальністю, а стан і дія нормуються так само, як для моделі динаміки
	for (int i = 0; i < COLLISION_RAYS; i++) input[i] = scan[i];

	normalizeInput(state, action, input + COLLISION_RAYS);
}

// Повертає ймовірність зіткнення протягом горизонту, якщо автомобіль з таким оточенням і рухом виконуватиме дію
float LearnedDynamics::predictCollision(const float* scan, const float* state, const float* action) const
{
	if (!hasCollisionModel()) return 0.0f;

	float input[COLLISION_RAYS + 6];
	float logit = 0.0f;

	collisionInput(scan, state, action, input);
	mCollisionNetwork.forward(input, &logit);

	return 1.0f / (1.0f + std::exp(-logit));
}

// Робить крок оптимізатора моделі зіткнень на пакеті прикладів і повертає середню логістичну похибку
float LearnedDynamics::trainCollisionBatch(const CollisionSample* const* batch, int count, AdamOptimizer& optimizer)
{
	if (count <= 0 || !hasCollisionModel()) return 0.0f;

	mGradients.assign(mCollisionNetwork.getParameterCount(), 0.0f);

	float input[COLLISION_RAYS + 6];
	float loss = 0.0f;

	for (int k = 0; k < count; k++)
	{
		collisionInput(batch[k]->scan, batch[k]->state, batch[k]->action, input);
		loss += mCollisionNetwork.accumulateLogisticGradient(input, batch[k]->label, mGradients.data());
	}

	float scale = 1.0f / (float)count;

	for (float& g : mGradients) g *= scale;

	optimizer.step(mCollisionNetwork.getParameterData(), mGradients.data());

	return loss / (float)count;
}

// Оцінює модель зіткнень: середня логістична похибка, частка вгаданих зіткнень і частка справжніх серед передбачених
void LearnedDynamics::evaluateCollision(const CollisionSample* const* batch, int count, float& loss, float& recall, float& precision) const
{
	loss = 0.0f;
	recall = 0.0f;
	precision = 0.0f;

	if (count <= 0 || !hasCollisionModel()) return;

	int truePositive = 0;
	int positives = 0;
	int predicted = 0;

	for (int k = 0; k < count; k++)
	{
		float p = predictCollision(batch[k]->scan, batch[k]->state, batch[k]->action);
		float t = batch[k]->label;

		loss -= t * std::log(p + 1e-6f) + (1.0f - t) * std::log(1.0f - p + 1e-6f);

		bool predictedHit = p > 0.5f;
		bool actualHit = t > 0.5f;

		if (actualHit) positives++;
		if (predictedHit) predicted++;
		if (predictedHit && actualHit) truePositive++;
	}

	loss /= (float)count;
	recall = positives > 0 ? (float)truePositive / positives : 0.0f;
	precision = predicted > 0 ? (float)truePositive / predicted : 0.0f;
}

// Записує модель у файл ресурсу
bool LearnedDynamics::save(const std::string& path) const
{
	std::error_code error;
	filesystem::path file(path);

	if (file.has_parent_path()) filesystem::create_directories(file.parent_path(), error);

	// Тимчасовий файл не дає агенту прочитати модель наполовину записаною
	std::string temporary = path + ".tmp";

	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);

		if (!out) return false;

		int normalized = mNormalized ? 1 : 0;

		out.write(DYNAMICS_MAGIC, sizeof(DYNAMICS_MAGIC));
		out.write((const char*)&DYNAMICS_VERSION, sizeof(int));
		out.write((const char*)&normalized, sizeof(int));
		out.write((const char*)mInputMean, sizeof(mInputMean));
		out.write((const char*)mInputScale, sizeof(mInputScale));
		out.write((const char*)mOutputMean, sizeof(mOutputMean));
		out.write((const char*)mOutputScale, sizeof(mOutputScale));
		out.write((const char*)&mTrainedSamples, sizeof(mTrainedSamples));
		out.write((const char*)&mHorizonError, sizeof(mHorizonError));
		out.write((const char*)&mHorizon, sizeof(mHorizon));

		if (!mNetwork.write(out)) return false;

		// Модель зіткнень необов'язкова; разом з нею записується огляд, на якому вона навчалася
		int collision = hasCollisionModel() ? 1 : 0;
		int rays = COLLISION_RAYS;
		float range = COLLISION_RANGE;
		int horizon = COLLISION_HORIZON;

		out.write((const char*)&collision, sizeof(int));

		if (collision)
		{
			out.write((const char*)&rays, sizeof(int));
			out.write((const char*)&range, sizeof(float));
			out.write((const char*)&horizon, sizeof(int));

			if (!mCollisionNetwork.write(out)) return false;
		}
	}

	filesystem::rename(temporary, file, error);

	return !error;
}

// Читає модель з файлу ресурсу
bool LearnedDynamics::load(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);

	if (!in) return false;

	char magic[4] = {};
	int version = 0;
	int normalized = 0;
	LearnedDynamics loaded;

	in.read(magic, sizeof(magic));
	in.read((char*)&version, sizeof(int));

	if (!in || std::memcmp(magic, DYNAMICS_MAGIC, sizeof(magic)) != 0 || version < 1 || version > DYNAMICS_VERSION) return false;

	in.read((char*)&normalized, sizeof(int));
	in.read((char*)loaded.mInputMean, sizeof(loaded.mInputMean));
	in.read((char*)loaded.mInputScale, sizeof(loaded.mInputScale));
	in.read((char*)loaded.mOutputMean, sizeof(loaded.mOutputMean));
	in.read((char*)loaded.mOutputScale, sizeof(loaded.mOutputScale));
	in.read((char*)&loaded.mTrainedSamples, sizeof(loaded.mTrainedSamples));
	in.read((char*)&loaded.mHorizonError, sizeof(loaded.mHorizonError));
	in.read((char*)&loaded.mHorizon, sizeof(loaded.mHorizon));

	if (!in || !loaded.mNetwork.read(in) || !loaded.isReady()) return false;

	// Перша версія моделі зіткнень не мала
	if (version >= 2)
	{
		int collision = 0;
		in.read((char*)&collision, sizeof(int));

		if (collision)
		{
			int rays = 0;
			int horizon = 0;
			float range = 0.0f;

			in.read((char*)&rays, sizeof(int));
			in.read((char*)&range, sizeof(float));
			in.read((char*)&horizon, sizeof(int));

			// Модель, навчена на іншому огляді, не підходить до променів, які пускає рушій
			if (!in || rays != COLLISION_RAYS || range != COLLISION_RANGE || !loaded.mCollisionNetwork.read(in)) return false;
		}
	}

	// Нульове відхилення у зіпсованому файлі дало б ділення на нуль
	for (float scale : loaded.mInputScale) if (!(scale > 0.0f)) return false;
	for (float scale : loaded.mOutputScale) if (!(scale > 0.0f)) return false;

	loaded.mNormalized = normalized != 0;

	*this = loaded;
	return true;
}
