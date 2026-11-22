#include "DynamicsTrainer.h"
#include "VehicleController.h"
#include "AgentSensor.h"
#include "RaySensor.h"
#include "AgentBrain.h"
#include "NeuroevolutionBrain.h"
#include "Prefab.h"
#include "RigidBody.h"
#include "EntityManager.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "DynamicsModel.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>

// Найбільша кількість перевірних переходів
static const size_t VALIDATION_CAPACITY = 30000;
// Найбільша кількість перевірних траєкторій
static const size_t TRAJECTORY_CAPACITY = 300;
// Скільки перевірних переходів брати для оцінки похибки одного кроку
static const int VALIDATION_SAMPLE = 3000;
// Скільки відрізків траєкторій брати для оцінки похибки на горизонті
static const int HORIZON_SEGMENTS = 300;
// Як часто перевіряти модель, с
static const double EVALUATION_INTERVAL = 2.0;

// Повертає поточний час у секундах для бюджету навчання й частоти перевірок
static double nowSeconds()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Повертає симуляції звичайний крок і малювання сцени
DynamicsTrainer::~DynamicsTrainer()
{
	SimulationSettings::fixedStepsPerFrame = 0;
	SimulationSettings::renderScene = true;
}

// Передає симуляції темп і малювання, навчає мережу в межах бюджету кадру та показує вікно
void DynamicsTrainer::update()
{
	SimulationSettings::fixedStepsPerFrame = stepsPerFrame > 1 ? stepsPerFrame : 0;
	SimulationSettings::renderScene = renderScene;

	if (mInitialized && !mFailed)
	{
		train();

		if (nowSeconds() - mLastEvaluation >= EVALUATION_INTERVAL) evaluate();
	}

	drawWindow();
}

// Створює водіїв і модель; повертає false, якщо навчання неможливо налаштувати
bool DynamicsTrainer::initialize()
{
	mInitialized = true;

	if (!agentPrefab)
	{
		std::cout << "DynamicsTrainer: needs an agent prefab" << std::endl;
		return false;
	}

	mRandom.seed(seed != 0 ? (unsigned int)seed : std::random_device()());

	// Продовження навчання бере і ваги, і нормування з файлу; інакше модель нова
	if (!continueTraining || !mModel.load(modelAsset))
	{
		mModel.create(mRandom);
	}

	// Модель зіткнень з'являється й у старій моделі динаміки, що її ще не мала
	if (!mModel.hasCollisionModel()) mModel.createCollisionModel(mRandom);

	mOptimizer.init(mModel.getNetwork().getParameterCount(), learningRate);
	mCollisionOptimizer.init(mModel.getCollisionNetwork().getParameterCount(), learningRate);

	AgentSensor::collectRoutes(route, mRoutes);

	bool policyAvailable = !policyWeights.empty() && !mRoutes.empty();
	int policyDrivers = policyAvailable ? (int)std::round(std::max(0.0f, std::min(policyShare, 1.0f)) * vehicleCount) : 0;

	// Частина випадкових водіїв їздить трасами: там вони б'ються об стіни й перешкоди, і модель зіткнень бачить удари
	int randomDrivers = std::max(1, vehicleCount) - policyDrivers;
	int trackDrivers = mRoutes.empty() ? 0 : (int)std::round(std::max(0.0f, std::min(randomOnTracks, 1.0f)) * randomDrivers);

	for (int i = 0; i < std::max(1, vehicleCount); i++)
	{
		Entity* entity = agentPrefab->instantiate();
		entity->setName("Driver " + std::to_string(i));

		Driver driver;
		driver.entity = entity;
		driver.vehicle = entity->getComponent<VehicleController>();
		driver.body = entity->getComponent<RigidBody>();

		if (!driver.vehicle || !driver.body)
		{
			std::cout << "DynamicsTrainer: agent prefab needs a RigidBody and a VehicleController" << std::endl;
			entity->destroy();
			return false;
		}

		driver.vehicle->playerInput = false;
		driver.vehicle->debugWindow = false;
		driver.body->setCollisionGroup((unsigned int)std::max(0, collisionGroup));
		driver.body->setReportCollisions(true);

		bool policy = i < policyDrivers;
		driver.onTrack = !policy && i < policyDrivers + trackDrivers;

		// Випадковому водієві дії задає тренер, тож мозок з образу йому лише заважав би
		std::vector<AgentBrain*> brains;

		for (Component* component : entity->getComponentList())
		{
			if (AgentBrain* brain = dynamic_cast<AgentBrain*>(component)) brains.push_back(brain);
		}

		if (policy)
		{
			if (!entity->getComponent<RaySensor>()) entity->addComponent<RaySensor>();

			driver.sensor = entity->getComponent<AgentSensor>();
			if (!driver.sensor) driver.sensor = entity->addComponent<AgentSensor>();

			driver.brain = entity->getComponent<NeuroevolutionBrain>();
			if (!driver.brain) driver.brain = entity->addComponent<NeuroevolutionBrain>();

			driver.brain->weights = policyWeights;
			driver.brain->episodic = false;

			for (AgentBrain* brain : brains)
			{
				if (brain != driver.brain) entity->removeComponent(brain);
			}
		}
		else
		{
			for (AgentBrain* brain : brains) entity->removeComponent(brain);
		}

		mDrivers.push_back(driver);
	}

	for (Driver& driver : mDrivers) respawn(driver);

	mTrainData.reserve(std::min(datasetCapacity, 1 << 20));
	mLastEvaluation = nowSeconds();

	return true;
}

// Ставить водія на початок нового епізоду
void DynamicsTrainer::respawn(Driver& driver)
{
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);

	Vector3 position;
	float heading = 0.0f;

	if (driver.brain || driver.onTrack)
	{
		// Водій на трасі стартує з випадкової точки випадкового маршруту, звідки лишається хоч третина шляху
		Entity* chosen = mRoutes[std::uniform_int_distribution<int>(0, (int)mRoutes.size() - 1)(mRandom)];

		std::vector<Vector3> waypoints;
		AgentSensor::collectWaypoints(chosen, waypoints);

		// Маршрут без точок не дає де стартувати: водій лишається на місці до наступного епізоду
		if (waypoints.size() < 2) waypoints = { driver.entity->getTransform()->getPosition(), driver.entity->getTransform()->getPosition() + driver.entity->getTransform()->getForward() };

		int last = std::max(0, (int)(waypoints.size() * 2 / 3) - 1);
		int start = std::uniform_int_distribution<int>(0, std::max(0, std::min(last, (int)waypoints.size() - 2)))(mRandom);

		Vector3 direction = waypoints[start + 1] - waypoints[start];
		position = waypoints[start] + Vector3(0.0f, 0.5f, 0.0f);
		heading = std::atan2(direction.x, direction.z);

		if (driver.sensor)
		{
			driver.sensor->route = chosen;
			driver.sensor->resetRoute(start);
		}
	}
	else
	{
		float half = fieldSize * 0.4f;
		position = fieldCenter + Vector3((unit(mRandom) * 2.0f - 1.0f) * half, 0.0f, (unit(mRandom) * 2.0f - 1.0f) * half);
		heading = unit(mRandom) * 6.2831853f;
	}

	driver.entity->isActiveSelf = true;
	driver.body->syncActivity();

	Transform* transform = driver.entity->getTransform();
	transform->setPosition(position);
	transform->setRotation(Vector3(0.0f, heading, 0.0f));

	driver.body->setVelocity(Vector3());
	driver.body->setAngularVelocity(Vector3());
	driver.body->takeCollisions();

	driver.vehicle->resetState();

	if (driver.brain) driver.brain->beginEpisode();

	// Приклади, горизонт яких не встиг минути, лишаються без мітки й відкидаються
	driver.pending.clear();
	driver.hasState = false;
	driver.steps = 0;
	driver.slowSteps = 0;
	driver.throttle = 0.0f;
	driver.steering = 0.0f;
	driver.targetTimer = 0;

	// Частина епізодів цілком іде на перевірку, тож перевірні траєкторії ніколи не потрапляють у навчання
	driver.trajectory = -1;

	if (unit(mRandom) < validationShare)
	{
		if (mTrajectories.size() < TRAJECTORY_CAPACITY)
		{
			mTrajectories.emplace_back();
			driver.trajectory = (int)mTrajectories.size() - 1;
		}
		else
		{
			int slot = (int)mTrajectoryNext;
			mTrajectoryNext = (mTrajectoryNext + 1) % TRAJECTORY_CAPACITY;

			// Траєкторію, яку зараз записує інший водій, не віддаємо: інакше вона змішала б два епізоди
			bool busy = false;

			for (const Driver& other : mDrivers)
			{
				if (&other != &driver && other.trajectory == slot) busy = true;
			}

			if (!busy)
			{
				driver.trajectory = slot;
				mTrajectories[slot].clear();
			}
		}
	}
}

// Вибирає випадковим водієві нові цільові дії
void DynamicsTrainer::pickTargets(Driver& driver)
{
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);

	// Переважно газ уперед, іноді гальмування чи задній хід і накат, щоб охопити різні режими руху
	float mode = unit(mRandom);

	if (mode < 0.7f) driver.throttleTarget = 0.2f + 0.8f * unit(mRandom);
	else if (mode < 0.85f) driver.throttleTarget = -unit(mRandom);
	else driver.throttleTarget = 0.2f * unit(mRandom) - 0.1f;

	driver.steeringTarget = unit(mRandom) < 0.3f ? 0.0f : unit(mRandom) * 2.0f - 1.0f;
	driver.targetTimer = std::uniform_int_distribution<int>(20, 120)(mRandom);
}

// Додає перехід до навчальних або перевірних даних
void DynamicsTrainer::storeTransition(const Driver& driver, const float* action, const float* next)
{
	DynamicsTransition transition;

	std::copy(driver.state, driver.state + 4, transition.state);
	std::copy(action, action + 2, transition.action);
	std::copy(next, next + 4, transition.next);

	mCollected++;

	if (driver.trajectory >= 0)
	{
		TrajectoryPoint point;
		std::copy(driver.state, driver.state + 4, point.state);
		std::copy(action, action + 2, point.action);
		point.x = driver.x;
		point.z = driver.z;
		point.heading = driver.heading;
		mTrajectories[driver.trajectory].push_back(point);

		if (mValidationData.size() < VALIDATION_CAPACITY) mValidationData.push_back(transition);
		else
		{
			mValidationData[mValidationNext] = transition;
			mValidationNext = (mValidationNext + 1) % VALIDATION_CAPACITY;
		}

		return;
	}

	size_t capacity = (size_t)std::max(batchSize, datasetCapacity);

	if (mTrainData.size() < capacity) mTrainData.push_back(transition);
	else
	{
		mTrainData[mTrainNext] = transition;
		mTrainNext = (mTrainNext + 1) % capacity;
	}
}

// Додає приклад з відомою міткою до навчальних або перевірних даних моделі зіткнень
void DynamicsTrainer::storeCollision(const Driver& driver, const PendingCollision& pending, float label)
{
	CollisionSample sample = pending.sample;
	int count = std::max(1, pending.age);

	// Дія прикладу - середня за час від його створення: вона й привела або не привела до удару
	sample.action[0] = pending.actionSum[0] / count;
	sample.action[1] = pending.actionSum[1] / count;
	sample.label = label;

	bool validation = driver.trajectory >= 0;
	std::vector<CollisionSample>& data = validation ? mCollisionValidation : mCollisionData;
	size_t& next = validation ? mCollisionValidationNext : mCollisionNext;
	size_t capacity = validation ? VALIDATION_CAPACITY : (size_t)std::max(batchSize, datasetCapacity);

	if (data.size() < capacity) data.push_back(sample);
	else
	{
		data[next] = sample;
		next = (next + 1) % capacity;
	}

	if (!validation)
	{
		mCollisionTotal++;
		if (label > 0.5f) mCollisionPositives++;
	}
}

// Оновлює приклади водія, що чекають на мітку: удар позначає всі, а спливлий горизонт - найстаріші як безпечні
void DynamicsTrainer::updatePending(Driver& driver, const float* action, bool collided)
{
	for (PendingCollision& pending : driver.pending)
	{
		pending.actionSum[0] += action[0];
		pending.actionSum[1] += action[1];
		pending.age++;
	}

	// Удар стався в межах горизонту кожного прикладу, що ще чекає
	if (collided)
	{
		for (const PendingCollision& pending : driver.pending) storeCollision(driver, pending, 1.0f);

		driver.pending.clear();
		return;
	}

	// Приклади, горизонт яких минув без удару, - безпечні
	size_t done = 0;

	while (done < driver.pending.size() && driver.pending[done].age >= LearnedDynamics::COLLISION_HORIZON)
	{
		storeCollision(driver, driver.pending[done], 0.0f);
		done++;
	}

	driver.pending.erase(driver.pending.begin(), driver.pending.begin() + done);
}

// Записує переходи всіх водіїв і задає дії випадковим водіям
void DynamicsTrainer::fixedUpdate()
{
	if (mFailed) return;

	if (!mInitialized)
	{
		if (!initialize()) mFailed = true;
		return;
	}

	std::uniform_real_distribution<float> noise(-1.0f, 1.0f);

	for (Driver& driver : mDrivers)
	{
		if (!EntityManager::get()->isAlive(driver.entity))
		{
			std::cout << "DynamicsTrainer: a driver was destroyed, data collection stopped" << std::endl;
			mFailed = true;
			return;
		}

		// Автомобіль збирається на першому кроці; до того записувати нічого
		if (!driver.vehicle->getRigidBody()) continue;

		bool collided = driver.body->takeCollisions() > 0;
		bool upright = driver.vehicle->isUpright();

		float state[4];
		float x, z, heading;
		driver.vehicle->getDynamicsState(state);
		driver.vehicle->getPlanarPose(x, z, heading);

		// Дія, з якою автомобіль пройшов попередній крок, - та, що зараз стоїть у керуванні
		float action[2] = { driver.vehicle->getThrottleAction(), driver.vehicle->getSteeringAction() };

		// Удари й перевертання модель не описує, тож такі переходи не записуються, а епізод завершується
		bool broken = collided || !upright;

		if (driver.hasState && !broken) storeTransition(driver, action, state);

		// Удар дає приклади моделі зіткнень; перевертання без удару - ні, бо що до нього призвело, невідомо
		if (collided || upright) updatePending(driver, action, collided);

		driver.steps++;
		driver.slowSteps = std::fabs(state[0]) < 0.3f ? driver.slowSteps + 1 : 0;

		bool outside = !driver.brain && !driver.onTrack && (std::fabs(x - fieldCenter.x) > fieldSize * 0.5f || std::fabs(z - fieldCenter.z) > fieldSize * 0.5f);
		bool stuck = driver.slowSteps * SimulationSettings::fixedStep > 3.0f;
		bool finished = driver.brain && driver.brain->isEpisodeDone();

		if (broken || outside || stuck || finished || driver.steps >= episodeSteps)
		{
			respawn(driver);
			continue;
		}

		std::copy(state, state + 4, driver.state);
		driver.x = x;
		driver.z = z;
		driver.heading = heading;
		driver.hasState = true;

		// Новий приклад для моделі зіткнень: огляд і стан зараз, а мітку визначить найближчий горизонт
		PendingCollision pending;
		DynamicsModel::scanSurroundings(driver.vehicle, pending.sample.scan);
		std::copy(state, state + 4, pending.sample.state);
		driver.pending.push_back(pending);

		// Випадкові дії плавно тягнуться до цілей, що час від часу змінюються
		if (!driver.brain)
		{
			if (--driver.targetTimer <= 0) pickTargets(driver);

			driver.throttle += (driver.throttleTarget - driver.throttle) * 0.08f + noise(mRandom) * 0.03f;
			driver.steering += (driver.steeringTarget - driver.steering) * 0.08f + noise(mRandom) * 0.05f;
			driver.throttle = std::max(-1.0f, std::min(driver.throttle, 1.0f));
			driver.steering = std::max(-1.0f, std::min(driver.steering, 1.0f));

			driver.vehicle->setActions(driver.throttle, driver.steering);
		}
	}
}

// Навчає мережу, доки не вичерпано бюджет кадру
void DynamicsTrainer::train()
{
	if ((int)mTrainData.size() < std::max(warmupSamples, batchSize)) return;

	// Нормування береться з першої зібраної вибірки й далі не змінюється, бо мережа навчається саме під нього
	if (!mModel.hasNormalization()) mModel.computeNormalization(mTrainData);

	mOptimizer.learningRate = learningRate;

	double start = nowSeconds();
	std::uniform_int_distribution<size_t> pick(0, mTrainData.size() - 1);

	do
	{
		mBatch.resize(batchSize);

		for (int i = 0; i < batchSize; i++) mBatch[i] = &mTrainData[pick(mRandom)];

		float loss = mModel.trainBatch(mBatch.data(), batchSize, mOptimizer);

		mTrainLoss = mBatches == 0 ? loss : mTrainLoss * 0.98f + loss * 0.02f;
		mBatches++;

		// Модель зіткнень навчається поруч, щойно серед прикладів з'явилися удари
		if ((int)mCollisionData.size() >= batchSize && mCollisionPositives > 0)
		{
			mCollisionOptimizer.learningRate = learningRate;
			mCollisionBatch.resize(batchSize);

			std::uniform_int_distribution<size_t> pickCollision(0, mCollisionData.size() - 1);

			for (int i = 0; i < batchSize; i++) mCollisionBatch[i] = &mCollisionData[pickCollision(mRandom)];

			float collisionLoss = mModel.trainCollisionBatch(mCollisionBatch.data(), batchSize, mCollisionOptimizer);

			mCollisionTrainLoss = mCollisionBatches == 0 ? collisionLoss : mCollisionTrainLoss * 0.98f + collisionLoss * 0.02f;
			mCollisionBatches++;
		}
	} while ((nowSeconds() - start) * 1000.0 < trainMilliseconds);
}

// Повертає середню похибку положення на горизонті H на перевірних траєкторіях
float DynamicsTrainer::horizonError(int horizon, int segments)
{
	std::vector<int> usable;

	for (size_t i = 0; i < mTrajectories.size(); i++)
	{
		if ((int)mTrajectories[i].size() > horizon) usable.push_back((int)i);
	}

	if (usable.empty()) return -1.0f;

	// Випадкові відрізки траєкторій прогнозуються разом, одним пакетом на крок
	std::vector<const std::vector<TrajectoryPoint>*> chosen(segments);
	std::vector<int> starts(segments);
	std::vector<float> states((size_t)segments * 4), next((size_t)segments * 4), actions((size_t)segments * 2);
	std::vector<float> xs(segments), zs(segments), headings(segments);

	for (int n = 0; n < segments; n++)
	{
		chosen[n] = &mTrajectories[usable[std::uniform_int_distribution<int>(0, (int)usable.size() - 1)(mRandom)]];
		starts[n] = std::uniform_int_distribution<int>(0, (int)chosen[n]->size() - horizon - 1)(mRandom);

		const TrajectoryPoint& first = (*chosen[n])[starts[n]];
		std::copy(first.state, first.state + 4, states.data() + n * 4);
		xs[n] = first.x;
		zs[n] = first.z;
		headings[n] = first.heading;
	}

	// Прогноз іде з відомими діями, але без поправок від справжнього стану: похибка накопичується так само, як під час планування
	double total = 0.0;

	for (int h = 1; h <= horizon; h++)
	{
		for (int n = 0; n < segments; n++)
		{
			const TrajectoryPoint& applied = (*chosen[n])[starts[n] + h - 1];
			actions[n * 2] = applied.action[0];
			actions[n * 2 + 1] = applied.action[1];
		}

		mModel.predictBatch(states.data(), actions.data(), segments, next.data());

		for (int n = 0; n < segments; n++)
		{
			LearnedDynamics::integratePose(xs[n], zs[n], headings[n], next.data() + n * 4, SimulationSettings::fixedStep);

			const TrajectoryPoint& actual = (*chosen[n])[starts[n] + h];
			total += std::sqrt((xs[n] - actual.x) * (xs[n] - actual.x) + (zs[n] - actual.z) * (zs[n] - actual.z));
		}

		states.swap(next);
	}

	return (float)(total / ((double)segments * horizon));
}

// Обчислює похибку одного кроку та положення на горизонті на перевірних даних і зберігає модель, якщо вона покращилася
void DynamicsTrainer::evaluate()
{
	mLastEvaluation = nowSeconds();

	if (mBatches == 0 || mValidationData.empty()) return;

	int count = std::min((int)mValidationData.size(), VALIDATION_SAMPLE);
	std::uniform_int_distribution<size_t> pick(0, mValidationData.size() - 1);

	mBatch.resize(count);

	for (int i = 0; i < count; i++) mBatch[i] = &mValidationData[pick(mRandom)];

	float validation = mModel.evaluate(mBatch.data(), count);
	float horizon = horizonError(std::max(1, evaluationHorizon), HORIZON_SEGMENTS);

	mValidationHistory.push_back(validation);

	// Модель зіткнень перевіряється на прикладах з перевірних епізодів
	bool collisionImproved = false;

	if (mCollisionBatches > 0 && !mCollisionValidation.empty())
	{
		int collisionCount = std::min((int)mCollisionValidation.size(), VALIDATION_SAMPLE);
		std::uniform_int_distribution<size_t> pickCollision(0, mCollisionValidation.size() - 1);

		mCollisionBatch.resize(collisionCount);

		for (int i = 0; i < collisionCount; i++) mCollisionBatch[i] = &mCollisionValidation[pickCollision(mRandom)];

		float collisionLoss = 0.0f;
		mModel.evaluateCollision(mCollisionBatch.data(), collisionCount, collisionLoss, mCollisionRecall, mCollisionPrecision);
		mCollisionLossHistory.push_back(collisionLoss);

		if (mBestCollisionLoss < 0.0f || collisionLoss < mBestCollisionLoss)
		{
			mBestCollisionLoss = collisionLoss;
			collisionImproved = true;
		}
	}

	if (horizon >= 0.0f) mHorizonHistory.push_back(horizon);

	bool dynamicsImproved = horizon >= 0.0f && (mBestHorizonError < 0.0f || horizon < mBestHorizonError);

	if (dynamicsImproved) mBestHorizonError = horizon;

	// Файл один на обидві моделі, тож записується, коли покращилася будь-яка з них
	if (dynamicsImproved || collisionImproved)
	{
		mModel.setStatistics(mModel.getTrainedSamples(), mBestHorizonError, evaluationHorizon);
		mSaved = mModel.save(modelAsset);

		std::cout << "Dynamics model: one-step error " << validation << ", position error over " << evaluationHorizon << " steps " << horizon << " m";
		if (!mCollisionLossHistory.empty()) std::cout << ", collision loss " << mCollisionLossHistory.back() << " recall " << mCollisionRecall << " precision " << mCollisionPrecision;
		std::cout << (mSaved ? ", saved" : ", not saved") << std::endl;
	}
}

// Малює вікно з ходом навчання
void DynamicsTrainer::drawWindow()
{
	ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Dynamics Model");

	if (mFailed)
	{
		ImGui::TextWrapped("Training is not running: check the agent prefab in the inspector.");
		ImGui::End();
		return;
	}

	ImGui::Text("Transitions: %d train, %d validation, %d trajectories", (int)mTrainData.size(), (int)mValidationData.size(), (int)mTrajectories.size());

	if ((int)mTrainData.size() < warmupSamples)
	{
		ImGui::Text("Collecting data before training: %d / %d", (int)mTrainData.size(), warmupSamples);
	}
	else
	{
		ImGui::Text("Batches: %lld, train error: %.4f", mBatches, mTrainLoss);
	}

	if (!mValidationHistory.empty())
	{
		float high = *std::max_element(mValidationHistory.begin(), mValidationHistory.end());

		ImGui::Text("One-step validation error: %.4f", mValidationHistory.back());
		ImGui::PlotLines("One-step", mValidationHistory.data(), (int)mValidationHistory.size(), 0, nullptr, 0.0f, high, ImVec2(0, 60));
	}

	if (!mHorizonHistory.empty())
	{
		float high = *std::max_element(mHorizonHistory.begin(), mHorizonHistory.end());

		ImGui::Text("Position error over %.1f s: %.3f m (best %.3f m)", evaluationHorizon * SimulationSettings::fixedStep, mHorizonHistory.back(), mBestHorizonError);
		ImGui::PlotLines("E(H), m", mHorizonHistory.data(), (int)mHorizonHistory.size(), 0, nullptr, 0.0f, high, ImVec2(0, 60));
	}

	ImGui::Separator();
	ImGui::Text("Collision samples: %lld (%.1f%% hits), %d validation", mCollisionTotal, mCollisionTotal > 0 ? 100.0 * mCollisionPositives / mCollisionTotal : 0.0, (int)mCollisionValidation.size());

	if (!mCollisionLossHistory.empty())
	{
		float high = *std::max_element(mCollisionLossHistory.begin(), mCollisionLossHistory.end());

		ImGui::Text("Collision loss: %.4f, hits caught: %.0f%%, alarms right: %.0f%%", mCollisionLossHistory.back(), mCollisionRecall * 100.0f, mCollisionPrecision * 100.0f);
		ImGui::PlotLines("Collision", mCollisionLossHistory.data(), (int)mCollisionLossHistory.size(), 0, nullptr, 0.0f, high, ImVec2(0, 60));
	}

	if (mSaved) ImGui::Text("Best model saved to %s", modelAsset.c_str());

	ImGui::SliderInt("Steps per frame", &stepsPerFrame, 1, 50);
	ImGui::SliderFloat("Training ms per frame", &trainMilliseconds, 1.0f, 50.0f);
	ImGui::Checkbox("Render scene", &renderScene);

	ImGui::End();
}

// Перелічує власні поля для файлу сцени та інспектора
void DynamicsTrainer::visitProperties(PropertyVisitor& visitor)
{
	visitor.reference("agentPrefab", agentPrefab);
	visitor.reference("route", route);
	visitor.asset("policyWeights", policyWeights, NeuralNetwork::FILE_EXTENSION);
	visitor.property("policyShare", policyShare, 0.01f);
	visitor.property("randomOnTracks", randomOnTracks, 0.01f);
	visitor.property("vehicleCount", vehicleCount);
	visitor.property("fieldCenter", fieldCenter);
	visitor.property("fieldSize", fieldSize, 1.0f);
	visitor.property("episodeSteps", episodeSteps);
	visitor.property("validationShare", validationShare, 0.01f);
	visitor.property("datasetCapacity", datasetCapacity);
	visitor.property("warmupSamples", warmupSamples);
	visitor.property("batchSize", batchSize);
	visitor.property("learningRate", learningRate, 0.0001f);
	visitor.property("trainMilliseconds", trainMilliseconds, 0.5f);
	visitor.property("evaluationHorizon", evaluationHorizon);
	visitor.property("continueTraining", continueTraining);
	visitor.property("collisionGroup", collisionGroup);
	visitor.property("seed", seed);
	visitor.property("stepsPerFrame", stepsPerFrame);
	visitor.property("renderScene", renderScene);
	visitor.property("modelAsset", modelAsset);

	vehicleCount = std::max(1, vehicleCount);
	episodeSteps = std::max(10, episodeSteps);
	datasetCapacity = std::max(1000, datasetCapacity);
	warmupSamples = std::max(0, warmupSamples);
	batchSize = std::max(1, std::min(batchSize, 4096));
	evaluationHorizon = std::max(1, evaluationHorizon);
	trainMilliseconds = std::max(0.0f, trainMilliseconds);
	stepsPerFrame = std::max(1, std::min(stepsPerFrame, 200));
}
