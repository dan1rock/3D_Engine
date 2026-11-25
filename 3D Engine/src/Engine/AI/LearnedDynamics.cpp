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
// Версія формату файлу моделі динаміки: 2 додала модель зіткнень, 3 - довільні розміри стану й дій
static const int DYNAMICS_VERSION = 3;
// Розмір стану в моделях першої та другої версій, зроблених для автомобіля
static const int LEGACY_STATE_SIZE = 4;
// Розмір дій у моделях першої та другої версій
static const int LEGACY_ACTION_SIZE = 2;
// Найменше відхилення, на яке ділиться величина під час нормування
static const float MIN_SCALE = 1e-4f;

const char* LearnedDynamics::FILE_EXTENSION = ".dyn";

// Створює нову модель для стану й дій заданих розмірів з випадковими вагами й тотожним нормуванням
void LearnedDynamics::create(int stateSize, int actionSize, std::mt19937& random)
{
	mStateSize = stateSize;
	mActionSize = actionSize;

	// Стан і дія на вході, два приховані шари по 128 нейронів, приріст стану на лінійному виході
	mNetwork.build({ stateSize + actionSize, 128, 128, stateSize }, true);
	mNetwork.randomize(random);

	mInputMean.assign(stateSize + actionSize, 0.0f);
	mInputScale.assign(stateSize + actionSize, 1.0f);
	mOutputMean.assign(stateSize, 0.0f);
	mOutputScale.assign(stateSize, 1.0f);

	mNormalized = false;
	mTrainedSamples = 0;
	mHorizonError = 0.0f;
	mHorizon = 0;
	mCollisionNetwork = NeuralNetwork();
	mCollisionRays = 0;
}

// Перевіряє, чи модель має мережу, що відповідає її розмірам
bool LearnedDynamics::isReady() const
{
	// Інтегрування пози бере з початку стану три швидкості, тож менший стан не підходить
	return mStateSize >= 3 && mActionSize >= 1 && mNetwork.getInputSize() == mStateSize + mActionSize && mNetwork.getOutputSize() == mStateSize;
}

// Повертає розмір стану
int LearnedDynamics::getStateSize() const
{
	return mStateSize;
}

// Повертає розмір дій
int LearnedDynamics::getActionSize() const
{
	return mActionSize;
}

// Повертає довжину запису переходу: стан, дія, наступний стан
int LearnedDynamics::getTransitionSize() const
{
	return mStateSize * 2 + mActionSize;
}

// Обчислює середні й відхилення входів і приростів за записами переходів, що лежать підряд
void LearnedDynamics::computeNormalization(const std::vector<float>& transitions)
{
	int stride = getTransitionSize();
	size_t count = stride > 0 ? transitions.size() / stride : 0;

	if (count == 0) return;

	int inputs = mStateSize + mActionSize;
	std::vector<double> inSum(inputs, 0.0), inSquares(inputs, 0.0), outSum(mStateSize, 0.0), outSquares(mStateSize, 0.0);

	for (size_t k = 0; k < count; k++)
	{
		const float* row = transitions.data() + k * stride;

		// Стан і дія лежать на початку запису підряд, як і на вході мережі
		for (int i = 0; i < inputs; i++)
		{
			inSum[i] += row[i];
			inSquares[i] += (double)row[i] * row[i];
		}

		for (int i = 0; i < mStateSize; i++)
		{
			double delta = (double)row[inputs + i] - row[i];
			outSum[i] += delta;
			outSquares[i] += delta * delta;
		}
	}

	double n = (double)count;

	for (int i = 0; i < inputs; i++)
	{
		double mean = inSum[i] / n;
		mInputMean[i] = (float)mean;
		mInputScale[i] = (float)std::max((double)MIN_SCALE, std::sqrt(std::max(0.0, inSquares[i] / n - mean * mean)));
	}

	for (int i = 0; i < mStateSize; i++)
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
	for (int i = 0; i < mStateSize; i++) input[i] = (state[i] - mInputMean[i]) / mInputScale[i];
	for (int i = 0; i < mActionSize; i++) input[mStateSize + i] = (action[i] - mInputMean[mStateSize + i]) / mInputScale[mStateSize + i];
}

// Нормує ціль навчання: приріст стану
void LearnedDynamics::normalizeTarget(const float* state, const float* next, float* target) const
{
	for (int i = 0; i < mStateSize; i++) target[i] = (next[i] - state[i] - mOutputMean[i]) / mOutputScale[i];
}

// Передбачає наступний стан за поточним станом і дією
void LearnedDynamics::predict(const float* state, const float* action, float* next) const
{
	mInput.resize(mStateSize + mActionSize);
	mOutput.resize(mStateSize);

	normalizeInput(state, action, mInput.data());
	mNetwork.forward(mInput.data(), mOutput.data());

	for (int i = 0; i < mStateSize; i++) next[i] = state[i] + mOutput[i] * mOutputScale[i] + mOutputMean[i];
}

// Передбачає наступні стани для count пар стан-дія підряд
void LearnedDynamics::predictBatch(const float* states, const float* actions, int count, float* next) const
{
	int inputs = mStateSize + mActionSize;

	mBatchInput.resize((size_t)count * inputs);
	mBatchOutput.resize((size_t)count * mStateSize);

	for (int s = 0; s < count; s++) normalizeInput(states + (size_t)s * mStateSize, actions + (size_t)s * mActionSize, mBatchInput.data() + (size_t)s * inputs);

	mNetwork.forwardBatch(mBatchInput.data(), count, mBatchOutput.data());

	for (int s = 0; s < count; s++)
	{
		for (int i = 0; i < mStateSize; i++)
		{
			size_t k = (size_t)s * mStateSize + i;
			next[k] = states[k] + mBatchOutput[k] * mOutputScale[i] + mOutputMean[i];
		}
	}
}

// Переносить положення й курс у площині на крок за станом, перші три значення якого - поздовжня, поперечна й кутова швидкості; курс 0 означає напрям уздовж осі Z
void LearnedDynamics::integratePose(float& x, float& z, float& heading, const float* state, float dt)
{
	heading += state[2] * dt;

	// Вперед - (sin, cos), праворуч - (cos, -sin): так рушій розташовує осі корпусу за курсом
	float s = std::sin(heading);
	float c = std::cos(heading);

	x += (s * state[0] + c * state[1]) * dt;
	z += (c * state[0] - s * state[1]) * dt;
}

// Робить крок оптимізатора на пакеті записів переходів і повертає середню нормовану похибку пакета
float LearnedDynamics::trainBatch(const float* const* batch, int count, AdamOptimizer& optimizer)
{
	if (count <= 0) return 0.0f;

	mGradients.assign(mNetwork.getParameterCount(), 0.0f);
	mInput.resize(mStateSize + mActionSize);
	mTarget.resize(mStateSize);

	float loss = 0.0f;

	for (int k = 0; k < count; k++)
	{
		const float* row = batch[k];

		normalizeInput(row, row + mStateSize, mInput.data());
		normalizeTarget(row, row + mStateSize + mActionSize, mTarget.data());
		loss += mNetwork.accumulateGradient(mInput.data(), mTarget.data(), mGradients.data());
	}

	// Градієнт усереднюється за пакетом, тож темп навчання не залежить від його розміру
	float scale = 1.0f / (float)count;

	for (float& g : mGradients) g *= scale;

	optimizer.step(mNetwork.getParameterData(), mGradients.data());

	mTrainedSamples += count;

	return loss / (float)count;
}

// Повертає середню нормовану похибку одного кроку на записах переходів
float LearnedDynamics::evaluate(const float* const* batch, int count) const
{
	if (count <= 0) return 0.0f;

	mInput.resize(mStateSize + mActionSize);
	mTarget.resize(mStateSize);
	mOutput.resize(mStateSize);

	float loss = 0.0f;

	for (int k = 0; k < count; k++)
	{
		const float* row = batch[k];

		normalizeInput(row, row + mStateSize, mInput.data());
		normalizeTarget(row, row + mStateSize + mActionSize, mTarget.data());
		mNetwork.forward(mInput.data(), mOutput.data());

		for (int i = 0; i < mStateSize; i++) loss += (mOutput[i] - mTarget[i]) * (mOutput[i] - mTarget[i]) / (float)mStateSize;
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

// Створює нову модель зіткнень з випадковими вагами: rays променів огляду дальністю range, удар передбачається на horizon кроків
void LearnedDynamics::createCollisionModel(int rays, float range, int horizon, std::mt19937& random)
{
	mCollisionRays = std::max(1, rays);
	mCollisionRange = std::max(0.1f, range);
	mCollisionHorizon = std::max(1, horizon);

	// Огляд, стан і дія на вході, два приховані шари, логарифм шансів зіткнення на лінійному виході
	mCollisionNetwork.build({ mCollisionRays + mStateSize + mActionSize, 64, 64, 1 }, true);
	mCollisionNetwork.randomize(random);
}

// Перевіряє, чи є модель зіткнень
bool LearnedDynamics::hasCollisionModel() const
{
	return mCollisionRays > 0 && mCollisionNetwork.getInputSize() == mCollisionRays + mStateSize + mActionSize && mCollisionNetwork.getOutputSize() == 1;
}

// Повертає кількість променів огляду моделі зіткнень
int LearnedDynamics::getCollisionRays() const
{
	return mCollisionRays;
}

// Повертає дальність променів огляду моделі зіткнень
float LearnedDynamics::getCollisionRange() const
{
	return mCollisionRange;
}

// Повертає горизонт, на якому модель зіткнень передбачає удар, у кроках фізики
int LearnedDynamics::getCollisionHorizon() const
{
	return mCollisionHorizon;
}

// Повертає довжину запису прикладу зіткнень: огляд, стан, дія, мітка
int LearnedDynamics::getCollisionSampleSize() const
{
	return mCollisionRays + mStateSize + mActionSize + 1;
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
	for (int i = 0; i < mCollisionRays; i++) input[i] = scan[i];

	normalizeInput(state, action, input + mCollisionRays);
}

// Повертає ймовірність зіткнення протягом горизонту, якщо об'єкт з таким оточенням і рухом виконуватиме дію
float LearnedDynamics::predictCollision(const float* scan, const float* state, const float* action) const
{
	if (!hasCollisionModel()) return 0.0f;

	mInput.resize(mCollisionRays + mStateSize + mActionSize);

	float logit = 0.0f;

	collisionInput(scan, state, action, mInput.data());
	mCollisionNetwork.forward(mInput.data(), &logit);

	return 1.0f / (1.0f + std::exp(-logit));
}

// Робить крок оптимізатора моделі зіткнень на пакеті записів і повертає середню логістичну похибку
float LearnedDynamics::trainCollisionBatch(const float* const* batch, int count, AdamOptimizer& optimizer)
{
	if (count <= 0 || !hasCollisionModel()) return 0.0f;

	mGradients.assign(mCollisionNetwork.getParameterCount(), 0.0f);
	mInput.resize(mCollisionRays + mStateSize + mActionSize);

	float loss = 0.0f;
	int labelOffset = mCollisionRays + mStateSize + mActionSize;

	for (int k = 0; k < count; k++)
	{
		const float* row = batch[k];

		collisionInput(row, row + mCollisionRays, row + mCollisionRays + mStateSize, mInput.data());
		loss += mCollisionNetwork.accumulateLogisticGradient(mInput.data(), row[labelOffset], mGradients.data());
	}

	float scale = 1.0f / (float)count;

	for (float& g : mGradients) g *= scale;

	optimizer.step(mCollisionNetwork.getParameterData(), mGradients.data());

	return loss / (float)count;
}

// Оцінює модель зіткнень: середня логістична похибка, частка вгаданих зіткнень і частка справжніх серед передбачених
void LearnedDynamics::evaluateCollision(const float* const* batch, int count, float& loss, float& recall, float& precision) const
{
	loss = 0.0f;
	recall = 0.0f;
	precision = 0.0f;

	if (count <= 0 || !hasCollisionModel()) return;

	int truePositive = 0;
	int positives = 0;
	int predicted = 0;
	int labelOffset = mCollisionRays + mStateSize + mActionSize;

	for (int k = 0; k < count; k++)
	{
		const float* row = batch[k];

		float p = predictCollision(row, row + mCollisionRays, row + mCollisionRays + mStateSize);
		float t = row[labelOffset];

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
		out.write((const char*)&mStateSize, sizeof(int));
		out.write((const char*)&mActionSize, sizeof(int));
		out.write((const char*)mInputMean.data(), sizeof(float) * mInputMean.size());
		out.write((const char*)mInputScale.data(), sizeof(float) * mInputScale.size());
		out.write((const char*)mOutputMean.data(), sizeof(float) * mOutputMean.size());
		out.write((const char*)mOutputScale.data(), sizeof(float) * mOutputScale.size());
		out.write((const char*)&mTrainedSamples, sizeof(mTrainedSamples));
		out.write((const char*)&mHorizonError, sizeof(mHorizonError));
		out.write((const char*)&mHorizon, sizeof(mHorizon));

		if (!mNetwork.write(out)) return false;

		// Модель зіткнень необов'язкова; разом з нею записується огляд, на якому вона навчалася
		int collision = hasCollisionModel() ? 1 : 0;

		out.write((const char*)&collision, sizeof(int));

		if (collision)
		{
			out.write((const char*)&mCollisionRays, sizeof(int));
			out.write((const char*)&mCollisionRange, sizeof(float));
			out.write((const char*)&mCollisionHorizon, sizeof(int));

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

	// Перші дві версії зберігали лише модель автомобіля зі сталими розмірами
	loaded.mStateSize = LEGACY_STATE_SIZE;
	loaded.mActionSize = LEGACY_ACTION_SIZE;

	if (version >= 3)
	{
		in.read((char*)&loaded.mStateSize, sizeof(int));
		in.read((char*)&loaded.mActionSize, sizeof(int));
	}

	if (!in || loaded.mStateSize < 1 || loaded.mStateSize > 256 || loaded.mActionSize < 1 || loaded.mActionSize > 256) return false;

	int inputs = loaded.mStateSize + loaded.mActionSize;

	loaded.mInputMean.resize(inputs);
	loaded.mInputScale.resize(inputs);
	loaded.mOutputMean.resize(loaded.mStateSize);
	loaded.mOutputScale.resize(loaded.mStateSize);

	in.read((char*)loaded.mInputMean.data(), sizeof(float) * inputs);
	in.read((char*)loaded.mInputScale.data(), sizeof(float) * inputs);
	in.read((char*)loaded.mOutputMean.data(), sizeof(float) * loaded.mStateSize);
	in.read((char*)loaded.mOutputScale.data(), sizeof(float) * loaded.mStateSize);
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
			in.read((char*)&loaded.mCollisionRays, sizeof(int));
			in.read((char*)&loaded.mCollisionRange, sizeof(float));
			in.read((char*)&loaded.mCollisionHorizon, sizeof(int));

			if (!in || !loaded.mCollisionNetwork.read(in) || !loaded.hasCollisionModel() || !(loaded.mCollisionRange > 0.0f)) return false;
		}
	}

	// Нульове відхилення у зіпсованому файлі дало б ділення на нуль
	for (float scale : loaded.mInputScale) if (!(scale > 0.0f)) return false;
	for (float scale : loaded.mOutputScale) if (!(scale > 0.0f)) return false;

	loaded.mNormalized = normalized != 0;

	*this = loaded;
	return true;
}
