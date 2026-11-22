#include "TrajectoryPredictor.h"
#include "DynamicsModel.h"
#include "RigidBody.h"
#include "Entity.h"
#include "EntityManager.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Підпис формату файлу мережі прогнозу траєкторій
static const char TRAJECTORY_MAGIC[4] = { 'E', 'T', 'P', '1' };
// Версія формату файлу мережі прогнозу траєкторій
static const int TRAJECTORY_VERSION = 1;
// Масштаб положень на вході мережі, м
static const float INPUT_SCALE = 10.0f;
// Масштаб швидкості на вході мережі, м/с
static const float SPEED_SCALE = 20.0f;
// Масштаб зміщень на виході мережі, м
static const float OUTPUT_SCALE = 20.0f;
// Найбільша кількість навчальних прикладів
static const size_t TRAIN_CAPACITY = 200000;
// Найбільша кількість перевірних прикладів
static const size_t VALIDATION_CAPACITY = 20000;
// Частка відрізків руху, що йдуть на перевірку
static const float VALIDATION_SHARE = 0.15f;
// Розмір пакета для кроку оптимізатора
static const int BATCH_SIZE = 128;
// Як часто перевіряти точність методів, с
static const double EVALUATION_INTERVAL = 2.0;
// Стрибок положення за один інтервал, після якого рух вважається перенесенням, а не їздою, м
static const float TELEPORT_DISTANCE = 15.0f;

const char* TrajectoryPredictor::FILE_EXTENSION = ".traj";

// Прогнозувальник, що зараз працює в сцені
static TrajectoryPredictor* sActive = nullptr;

// Повертає поточний час у секундах
static double nowSeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Розміри шарів мережі: минулі положення й швидкість, два приховані шари, майбутні зміщення
static std::vector<int> trajectoryLayers()
{
	return { TrajectoryPredictor::HISTORY * 2 + 1, 128, 128, TrajectoryPredictor::FUTURE * 2 };
}

// Знімає себе з ролі активного прогнозувальника сцени
TrajectoryPredictor::~TrajectoryPredictor()
{
	if (sActive == this) sActive = nullptr;
}

// Стає активним прогнозувальником сцени
void TrajectoryPredictor::awake()
{
	sActive = this;
}

// Повертає прогнозувальник, що зараз працює в сцені, або nullptr
TrajectoryPredictor* TrajectoryPredictor::getActive()
{
	return sActive;
}

// Переводить зміщення зі світових осей у систему курсу
void TrajectoryPredictor::toHeadingFrame(float dx, float dz, float heading, float& lateral, float& forward)
{
	float s = std::sin(heading);
	float c = std::cos(heading);

	// Вперед - (sin, cos), праворуч - (cos, -sin), як осі корпусу в рушії
	forward = dx * s + dz * c;
	lateral = dx * c - dz * s;
}

// Перечитує мережу з файлу, якщо шлях змінився
void TrajectoryPredictor::ensureNetwork()
{
	if (mNetworkReady && mLoadedPath == modelAsset) return;

	mLoadedPath = modelAsset;
	mNetworkReady = false;

	std::ifstream in(modelAsset, std::ios::binary);
	char magic[4] = {};
	int version = 0;

	if (in)
	{
		in.read(magic, sizeof(magic));
		in.read((char*)&version, sizeof(int));
	}

	NeuralNetwork loaded;

	if (in && std::memcmp(magic, TRAJECTORY_MAGIC, sizeof(magic)) == 0 && version == TRAJECTORY_VERSION && loaded.read(in) && loaded.getLayers() == trajectoryLayers())
	{
		mNetwork = loaded;
		mNetworkReady = true;
	}

	// Навчання починається з випадкових ваг, якщо файлу немає або продовжувати його не просили
	if (train && (!mNetworkReady || !continueTraining))
	{
		mNetwork.build(trajectoryLayers(), true);
		mNetwork.randomize(mRandom);
		mNetworkReady = true;
	}

	if (train) mOptimizer.init(mNetwork.getParameterCount(), learningRate);
}

// Оновлює фільтр Калмана новим виміряним положенням з відомим відхиленням шуму
void TrajectoryPredictor::updateFilter(AxisFilter& f, float measurement, float noise)
{
	float dt = SAMPLE_INTERVAL;

	// Прогноз: положення зсувається на швидкість, а невизначеність зростає на шум прискорення
	float q = processNoise * processNoise;
	f.position += f.velocity * dt;

	float p00 = f.p00 + dt * (f.p01 + f.p01) + dt * dt * f.p11 + q * dt * dt * dt * dt / 4.0f;
	float p01 = f.p01 + dt * f.p11 + q * dt * dt * dt / 2.0f;
	float p11 = f.p11 + q * dt * dt;

	// Корекція виміряним положенням
	float r = std::max(noise, 0.05f);
	float s = p00 + r * r;
	float k0 = p00 / s;
	float k1 = p01 / s;
	float innovation = measurement - f.position;

	f.position += k0 * innovation;
	f.velocity += k1 * innovation;

	f.p00 = (1.0f - k0) * p00;
	f.p01 = (1.0f - k0) * p01;
	f.p11 = p11 - k1 * p01;
}

// Складає входи мережі з історії: минулі положення в системі курсу та швидкість
bool TrajectoryPredictor::buildInput(const Track& track, float* input) const
{
	int points = (int)track.history.size() / 2;

	if (points < HISTORY + 1) return false;

	const float* last = track.history.data() + (points - 1) * 2;

	for (int i = 0; i < HISTORY; i++)
	{
		const float* point = track.history.data() + (points - 1 - HISTORY + i) * 2;
		float lateral, forward;

		toHeadingFrame(point[0] - last[0], point[1] - last[1], track.heading, lateral, forward);

		input[i * 2] = lateral / INPUT_SCALE;
		input[i * 2 + 1] = forward / INPUT_SCALE;
	}

	// Швидкість береться з фільтра Калмана: різниця двох зашумлених положень дала б надто неточну
	float vx = track.filterX.velocity;
	float vz = track.filterZ.velocity;

	input[HISTORY * 2] = std::sqrt(vx * vx + vz * vz) / SPEED_SCALE;

	return true;
}

// Обчислює прогноз нейромережі для об'єкта на поточний запис
void TrajectoryPredictor::updateNeural(Track& track)
{
	if (track.neuralTick == mTick) return;

	track.neuralTick = mTick;

	float input[HISTORY * 2 + 1];
	float output[FUTURE * 2];

	if (!mNetworkReady || !buildInput(track, input))
	{
		std::fill(track.neural, track.neural + FUTURE * 2, 0.0f);
		return;
	}

	mNetwork.forward(input, output);

	float s = std::sin(track.heading);
	float c = std::cos(track.heading);

	// Зміщення з системи курсу назад у світові осі
	for (int i = 0; i < FUTURE; i++)
	{
		float lateral = output[i * 2] * OUTPUT_SCALE;
		float forward = output[i * 2 + 1] * OUTPUT_SCALE;

		track.neural[i * 2] = forward * s + lateral * c;
		track.neural[i * 2 + 1] = forward * c - lateral * s;
	}
}

// Прогнозує положення об'єкта через time секунд обраним методом; false, якщо об'єкт ще не відстежується
bool TrajectoryPredictor::predict(RigidBody* body, float time, Vector3& position)
{
	auto it = mTracks.find(body);

	if (it == mTracks.end() || it->second.lastSeen < mTick - 1 || it->second.history.size() < 4) return false;

	Track& track = it->second;
	int points = (int)track.history.size() / 2;
	const float* last = track.history.data() + (points - 1) * 2;
	const float* previous = last - 2;

	position = Vector3(last[0], track.height, last[1]);

	if (time <= 0.0f) return true;

	ensureNetwork();

	TrajectoryMethod chosen = (TrajectoryMethod)std::max(0, std::min(method, 2));

	if (chosen == TrajectoryMethod::Neural && (!mNetworkReady || points < HISTORY + 1)) chosen = TrajectoryMethod::Kalman;
	if (chosen == TrajectoryMethod::Kalman && !track.filterReady) chosen = TrajectoryMethod::ConstantVelocity;

	if (chosen == TrajectoryMethod::ConstantVelocity)
	{
		position.x += (last[0] - previous[0]) / SAMPLE_INTERVAL * time;
		position.z += (last[1] - previous[1]) / SAMPLE_INTERVAL * time;
		return true;
	}

	if (chosen == TrajectoryMethod::Kalman)
	{
		position.x = track.filterX.position + track.filterX.velocity * time;
		position.z = track.filterZ.position + track.filterZ.velocity * time;
		return true;
	}

	updateNeural(track);

	// Між передбаченими моментами положення інтерполюється, а за останнім - продовжується останнім відрізком
	float step = SAMPLE_INTERVAL * FUTURE_STRIDE;
	float index = time / step;
	int segment = std::min((int)index, FUTURE - 1);
	float t = index - segment;

	float ax = segment == 0 ? 0.0f : track.neural[(segment - 1) * 2];
	float az = segment == 0 ? 0.0f : track.neural[(segment - 1) * 2 + 1];
	float bx = track.neural[segment * 2];
	float bz = track.neural[segment * 2 + 1];

	position.x += ax + (bx - ax) * t;
	position.z += az + (bz - az) * t;

	return true;
}

// Записує положення об'єктів раз на інтервал і готує приклади
void TrajectoryPredictor::fixedUpdate()
{
	int interval = std::max(1, (int)std::round(SAMPLE_INTERVAL / SimulationSettings::fixedStep));

	if (++mStepCounter < interval) return;

	mStepCounter = 0;
	mTick++;

	std::normal_distribution<float> gaussian(0.0f, 1.0f);
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);

	for (const auto& entry : EntityManager::get()->getRigidBodies())
	{
		RigidBody* body = entry.second;

		if (body->isStatic() || !body->getOwner()->isActive()) continue;

		Vector3 position, right, up, forward;
		body->getPose(position, right, up, forward);

		Track& track = mTracks[body];

		// Без навчання шум задається повзунком і змінюється одразу
		if (!train) track.noise = measurementNoise;

		// Відрізок руху, що вже йде, зберігає свій шум; новий отримає шум нижче, тож тут беремо поточний
		float noiseLevel = track.lastSeen == mTick - 1 ? track.noise : 0.0f;
		float x = position.x + gaussian(mRandom) * noiseLevel;
		float z = position.z + gaussian(mRandom) * noiseLevel;

		// Об'єкт, якого не бачили на попередньому записі чи який стрибнув, почав новий відрізок руху
		bool continuous = track.lastSeen == mTick - 1 && !track.history.empty();

		if (continuous)
		{
			float jx = x - track.history[track.history.size() - 2];
			float jz = z - track.history[track.history.size() - 1];

			if (jx * jx + jz * jz > TELEPORT_DISTANCE * TELEPORT_DISTANCE) continuous = false;
		}

		if (!continuous)
		{
			track = Track();
			track.validation = unit(mRandom) < VALIDATION_SHARE;
			track.startTick = mTick;

			// Під час навчання кожен відрізок руху отримує свій випадковий шум, а без навчання - заданий
			track.noise = train && measurementNoise <= 0.0f ? unit(mRandom) * trainingNoise : measurementNoise;
			x = position.x + gaussian(mRandom) * track.noise;
			z = position.z + gaussian(mRandom) * track.noise;
		}

		track.trueX = position.x;
		track.trueZ = position.z;

		track.lastSeen = mTick;
		track.height = position.y;

		track.history.push_back(x);
		track.history.push_back(z);

		if ((int)track.history.size() > (HISTORY + 1) * 2) track.history.erase(track.history.begin(), track.history.begin() + 2);

		if (!track.filterReady)
		{
			track.filterX.position = x;
			track.filterZ.position = z;
			track.filterReady = true;
		}
		else
		{
			updateFilter(track.filterX, x, track.noise);
			updateFilter(track.filterZ, z, track.noise);
		}

		// Курс - напрям згладженої фільтром швидкості; майже нерухомий об'єкт зберігає попередній курс
		float filteredSpeed = std::sqrt(track.filterX.velocity * track.filterX.velocity + track.filterZ.velocity * track.filterZ.velocity);

		if (filteredSpeed > 0.5f) track.heading = std::atan2(track.filterX.velocity, track.filterZ.velocity);

		// Новий приклад: входи зараз, а правильні зміщення стануть відомі через два секунди
		PendingSample pending;

		if (!buildInput(track, pending.input)) continue;

		pending.body = body;
		pending.tick = mTick;
		pending.x = x;
		pending.z = z;
		pending.heading = track.heading;
		// Без навчання всі приклади йдуть на перевірку: так видно точність методів на поточній сцені
		pending.validation = track.validation || !train;
		pending.collected = 0;

		int points = (int)track.history.size() / 2;
		const float* previous = track.history.data() + (points - 2) * 2;
		float vx = (x - previous[0]) / SAMPLE_INTERVAL;
		float vz = (z - previous[1]) / SAMPLE_INTERVAL;

		for (int i = 0; i < FUTURE; i++)
		{
			float time = (i + 1) * FUTURE_STRIDE * SAMPLE_INTERVAL;

			toHeadingFrame(vx * time, vz * time, track.heading, pending.constant[i * 2], pending.constant[i * 2 + 1]);
			toHeadingFrame(track.filterX.position + track.filterX.velocity * time - x, track.filterZ.position + track.filterZ.velocity * time - z, track.heading, pending.kalman[i * 2], pending.kalman[i * 2 + 1]);
		}

		mPending.push_back(pending);
	}

	// Приклади отримують майбутні положення свого об'єкта; той, що зник чи стрибнув, відкидається
	for (size_t i = 0; i < mPending.size();)
	{
		PendingSample& pending = mPending[i];
		auto it = mTracks.find(pending.body);
		long long age = mTick - pending.tick;

		// Приклад втрачено, якщо об'єкт зник або почав новий відрізок руху після створення прикладу
		bool lost = it == mTracks.end() || it->second.lastSeen != mTick || it->second.startTick > pending.tick;

		if (!lost && age > 0 && age % FUTURE_STRIDE == 0)
		{
			// Правильна відповідь - справжнє майбутнє положення без шуму відносно виміряного тоді положення
			float dx = it->second.trueX - pending.x;
			float dz = it->second.trueZ - pending.z;

			toHeadingFrame(dx, dz, pending.heading, pending.target[pending.collected * 2], pending.target[pending.collected * 2 + 1]);
			pending.collected++;
		}

		if (lost || pending.collected >= FUTURE)
		{
			if (!lost)
			{
				Sample sample;
				std::memcpy(sample.input, pending.input, sizeof(sample.input));
				std::memcpy(sample.target, pending.target, sizeof(sample.target));
				std::memcpy(sample.constant, pending.constant, sizeof(sample.constant));
				std::memcpy(sample.kalman, pending.kalman, sizeof(sample.kalman));

				std::vector<Sample>& data = pending.validation ? mValidationData : mTrainData;
				size_t& next = pending.validation ? mValidationNext : mTrainNext;
				size_t capacity = pending.validation ? VALIDATION_CAPACITY : TRAIN_CAPACITY;

				if (data.size() < capacity) data.push_back(sample);
				else
				{
					data[next] = sample;
					next = (next + 1) % capacity;
				}
			}

			mPending[i] = mPending.back();
			mPending.pop_back();
			continue;
		}

		i++;
	}

	// Об'єкти, яких давно не видно, забуваються
	for (auto it = mTracks.begin(); it != mTracks.end();)
	{
		if (it->second.lastSeen < mTick - 50) it = mTracks.erase(it);
		else ++it;
	}
}

// Навчає мережу, доки не вичерпано бюджет кадру
void TrajectoryPredictor::trainNetwork()
{
	if (!train || mTrainData.size() < 2000) return;

	ensureNetwork();

	mOptimizer.learningRate = learningRate;

	std::uniform_int_distribution<size_t> pick(0, mTrainData.size() - 1);
	double start = nowSeconds();
	float target[FUTURE * 2];

	do
	{
		mGradients.assign(mNetwork.getParameterCount(), 0.0f);

		float loss = 0.0f;

		for (int k = 0; k < BATCH_SIZE; k++)
		{
			const Sample& sample = mTrainData[pick(mRandom)];

			for (int i = 0; i < FUTURE * 2; i++) target[i] = sample.target[i] / OUTPUT_SCALE;

			loss += mNetwork.accumulateGradient(sample.input, target, mGradients.data());
		}

		for (float& g : mGradients) g /= (float)BATCH_SIZE;

		mOptimizer.step(mNetwork.getParameterData(), mGradients.data());

		loss /= (float)BATCH_SIZE;
		mTrainLoss = mBatches == 0 ? loss : mTrainLoss * 0.98f + loss * 0.02f;
		mBatches++;
	} while ((nowSeconds() - start) * 1000.0 < trainMilliseconds);
}

// Обчислює точність методів на перевірних прикладах
void TrajectoryPredictor::evaluate()
{
	mLastEvaluation = nowSeconds();

	if (mValidationData.empty()) return;

	ensureNetwork();

	int count = std::min((int)mValidationData.size(), 3000);
	std::uniform_int_distribution<size_t> pick(0, mValidationData.size() - 1);

	double ade[3] = {}, fde[3] = {};
	float output[FUTURE * 2];

	for (int k = 0; k < count; k++)
	{
		const Sample& sample = mValidationData[pick(mRandom)];
		const float* predictions[3] = { sample.constant, sample.kalman, nullptr };

		if (mNetworkReady)
		{
			mNetwork.forward(sample.input, output);

			for (float& value : output) value *= OUTPUT_SCALE;

			predictions[2] = output;
		}

		for (int m = 0; m < 3; m++)
		{
			if (!predictions[m]) continue;

			double total = 0.0;
			double last = 0.0;

			for (int i = 0; i < FUTURE; i++)
			{
				float dx = predictions[m][i * 2] - sample.target[i * 2];
				float dz = predictions[m][i * 2 + 1] - sample.target[i * 2 + 1];
				last = std::sqrt(dx * dx + dz * dz);
				total += last;
			}

			ade[m] += total / FUTURE;
			fde[m] += last;
		}
	}

	for (int m = 0; m < 3; m++)
	{
		mAde[m] = (float)(ade[m] / count);
		mFde[m] = (float)(fde[m] / count);
	}

	if (!mNetworkReady)
	{
		mAde[2] = -1.0f;
		mFde[2] = -1.0f;
		return;
	}

	if (!train || mBatches == 0) return;

	mAdeHistory.push_back(mAde[2]);

	// Мережа зберігається, коли її середня похибка на перевірних прикладах стала найменшою
	if (mBestAde < 0.0f || mAde[2] < mBestAde)
	{
		mBestAde = mAde[2];

		std::error_code error;
		filesystem::path file(modelAsset);

		if (file.has_parent_path()) filesystem::create_directories(file.parent_path(), error);

		std::string temporary = modelAsset + ".tmp";

		{
			std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
			out.write(TRAJECTORY_MAGIC, sizeof(TRAJECTORY_MAGIC));
			out.write((const char*)&TRAJECTORY_VERSION, sizeof(int));
			mSaved = (bool)out && mNetwork.write(out);
		}

		if (mSaved)
		{
			filesystem::rename(temporary, file, error);
			mSaved = !error;
		}

		std::cout << "Trajectory prediction ADE/FDE: constant velocity " << mAde[0] << "/" << mFde[0] << ", Kalman " << mAde[1] << "/" << mFde[1] << ", network " << mAde[2] << "/" << mFde[2] << (mSaved ? ", saved" : "") << std::endl;
	}
}

// Навчає мережу, малює прогнози та вікно
void TrajectoryPredictor::update()
{
	trainNetwork();

	if (nowSeconds() - mLastEvaluation >= EVALUATION_INTERVAL) evaluate();

	if (showPredictions)
	{
		std::vector<Vector3> history;
		std::vector<Vector3> future;

		for (auto& entry : mTracks)
		{
			Track& track = entry.second;

			if (track.lastSeen != mTick || (int)track.history.size() < 4) continue;

			// Пройдений шлях - сірим, прогноз на дві секунди - фіолетовим
			history.clear();

			for (size_t i = 0; i + 1 < track.history.size(); i += 2) history.push_back(Vector3(track.history[i], track.height, track.history[i + 1]));

			DynamicsModel::drawPath(history, IM_COL32(170, 170, 170, 200), 1.5f);

			future.clear();

			for (int i = 0; i <= FUTURE; i++)
			{
				Vector3 point;

				if (predict(entry.first, i * FUTURE_STRIDE * SAMPLE_INTERVAL, point)) future.push_back(point);
			}

			DynamicsModel::drawPath(future, IM_COL32(220, 80, 255, 255), 2.0f);
		}
	}

	if (showWindow) drawWindow();
}

// Малює вікно з точністю методів
void TrajectoryPredictor::drawWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Trajectory Prediction");

	const char* names[3] = { "Constant velocity", "Kalman filter", "Neural network" };

	ImGui::Text("Tracked objects: %d", (int)mTracks.size());
	ImGui::Text("Samples: %d train, %d validation", (int)mTrainData.size(), (int)mValidationData.size());

	if (train) ImGui::Text("Batches: %lld, train error: %.5f", mBatches, mTrainLoss);

	// Таблиця точності: середня й кінцева похибки зміщення за дві секунди
	if (!mValidationData.empty() && ImGui::BeginTable("accuracy", 3, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Method");
		ImGui::TableSetupColumn("ADE, m");
		ImGui::TableSetupColumn("FDE, m");
		ImGui::TableHeadersRow();

		for (int m = 0; m < 3; m++)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(names[m]);
			ImGui::TableNextColumn();
			if (mAde[m] >= 0.0f) ImGui::Text("%.3f", mAde[m]); else ImGui::TextDisabled("-");
			ImGui::TableNextColumn();
			if (mFde[m] >= 0.0f) ImGui::Text("%.3f", mFde[m]); else ImGui::TextDisabled("-");
		}

		ImGui::EndTable();
	}

	if (!mAdeHistory.empty())
	{
		float high = *std::max_element(mAdeHistory.begin(), mAdeHistory.end());
		ImGui::PlotLines("Network ADE", mAdeHistory.data(), (int)mAdeHistory.size(), 0, nullptr, 0.0f, high, ImVec2(0, 60));
	}

	if (mSaved) ImGui::Text("Best network saved to %s", modelAsset.c_str());

	ImGui::Combo("Method", &method, names, 3);
	ImGui::SliderFloat("Measurement noise, m", &measurementNoise, 0.0f, 1.0f);
	ImGui::Checkbox("Show predictions", &showPredictions);

	ImGui::End();
}

// Перелічує власні поля для файлу сцени та інспектора
void TrajectoryPredictor::visitProperties(PropertyVisitor& visitor)
{
	visitor.property("method", method);
	visitor.asset("modelAsset", modelAsset, FILE_EXTENSION);
	visitor.property("train", train);
	visitor.property("continueTraining", continueTraining);
	visitor.property("measurementNoise", measurementNoise, 0.01f);
	visitor.property("trainingNoise", trainingNoise, 0.01f);
	visitor.property("processNoise", processNoise, 0.1f);
	visitor.property("trainMilliseconds", trainMilliseconds, 0.5f);
	visitor.property("learningRate", learningRate, 0.0001f);
	visitor.property("showPredictions", showPredictions);
	visitor.property("showWindow", showWindow);

	method = std::max(0, std::min(method, 2));
	measurementNoise = std::max(0.0f, measurementNoise);
	processNoise = std::max(0.01f, processNoise);
	trainMilliseconds = std::max(0.0f, trainMilliseconds);
}
