#include "DynamicsTrainer.h"
#include "Agent.h"
#include "AgentBrain.h"
#include "ControllableBody.h"
#include "DynamicsModel.h"
#include "Prefab.h"
#include "RigidBody.h"
#include "EntityManager.h"
#include "SimulationSettings.h"
#include "Properties.h"
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

// Дописує запис до сховища записів однакової довжини, а заповнене сховище оновлює по колу
static void storeRow(std::vector<float>& data, size_t& next, size_t capacity, const float* row, int stride)
{
	if (data.size() / stride < capacity)
	{
		data.insert(data.end(), row, row + stride);
		return;
	}

	std::copy(row, row + stride, data.begin() + next * stride);
	next = (next + 1) % capacity;
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

// Створює об'єкти й модель; повертає false, якщо навчання неможливо налаштувати
bool DynamicsTrainer::initialize()
{
	mInitialized = true;

	if (!agentPrefab)
	{
		std::cout << "DynamicsTrainer: needs an agent prefab" << std::endl;
		return false;
	}

	mRandom.seed(seed != 0 ? (unsigned int)seed : std::random_device()());

	int count = std::max(1, bodyCount);

	for (int i = 0; i < count; i++)
	{
		Entity* entity = agentPrefab->instantiate();
		entity->setName("Driver " + std::to_string(i));

		Driver driver;
		driver.entity = entity;
		driver.controllable = entity->getComponent<ControllableBody>();
		driver.body = entity->getComponent<RigidBody>();
		driver.agent = entity->getComponent<Agent>();

		if (!driver.controllable || !driver.body)
		{
			std::cout << "DynamicsTrainer: agent prefab needs a RigidBody and a component derived from ControllableBody" << std::endl;
			entity->destroy();
			return false;
		}

		driver.controllable->setTrainingMode(true);
		driver.body->setCollisionGroup((unsigned int)std::max(0, collisionGroup));
		driver.body->setReportCollisions(true);

		mDrivers.push_back(driver);
	}

	mStateSize = mDrivers.front().controllable->getStateSize();
	mActionSize = mDrivers.front().controllable->getActionSize();

	if (mStateSize < 3 || mActionSize < 1)
	{
		std::cout << "DynamicsTrainer: the controllable body needs at least three state values and one action" << std::endl;
		return false;
	}

	// Продовження навчання бере і ваги, і нормування з файлу, якщо модель зроблено для таких самих розмірів; інакше модель нова
	if (!continueTraining || !mModel.load(modelAsset) || mModel.getStateSize() != mStateSize || mModel.getActionSize() != mActionSize)
	{
		mModel.create(mStateSize, mActionSize, mRandom);
	}

	// Модель зіткнень з'являється й у збереженій моделі, що її ще не мала
	if (learnCollisions && !mModel.hasCollisionModel()) mModel.createCollisionModel(collisionRays, collisionRange, collisionHorizon, mRandom);

	mOptimizer.init(mModel.getNetwork().getParameterCount(), learningRate);
	if (mModel.hasCollisionModel()) mCollisionOptimizer.init(mModel.getCollisionNetwork().getParameterCount(), learningRate);

	// Мозком і середовищем можна скористатися лише тоді, коли образ має агента, а сцена - середовище
	Agent* agent = mDrivers.front().agent;
	bool canPlace = agent && environment;
	bool hasBrain = agent && agent->getBrain();

	mVariants = canPlace ? std::max(1, agent->getVariantCount(environment)) : 1;

	int policyDrivers = canPlace && hasBrain ? (int)std::round(std::max(0.0f, std::min(policyShare, 1.0f)) * count) : 0;
	int randomDrivers = count - policyDrivers;
	int environmentDrivers = canPlace ? (int)std::round(std::max(0.0f, std::min(environmentShare, 1.0f)) * randomDrivers) : 0;

	for (int i = 0; i < count; i++)
	{
		Driver& driver = mDrivers[i];

		driver.policy = i < policyDrivers;
		driver.inEnvironment = !driver.policy && i < policyDrivers + environmentDrivers;
		driver.state.assign(mStateSize, 0.0f);
		driver.actions.assign(mActionSize, 0.0f);
		driver.targets.assign(mActionSize, 0.0f);

		// Мозок агента керує лише своїм об'єктом і поза навчанням, тож епізод не обривається на першому ударі; решті дії задає тренер
		if (driver.agent)
		{
			driver.agent->setTraining(false);
			driver.agent->setControlEnabled(driver.policy);
		}
	}

	for (Driver& driver : mDrivers) respawn(driver);

	mLastEvaluation = nowSeconds();

	return true;
}

// Ставить об'єкт на початок нового епізоду
void DynamicsTrainer::respawn(Driver& driver)
{
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);

	// Актор має повернутися в сцену до того, як йому задаватимуть положення й швидкість
	driver.entity->isActiveSelf = true;
	driver.body->syncActivity();

	if (driver.policy || driver.inEnvironment)
	{
		// Агент сам стає у випадковий варіант середовища з випадковими початковими умовами
		EpisodeSetup setup;
		setup.environment = environment;
		setup.variant = std::uniform_int_distribution<int>(0, mVariants - 1)(mRandom);
		setup.seed = (unsigned int)mRandom();

		driver.agent->beginEpisode(setup);
	}
	else
	{
		float half = fieldSize * 0.4f;
		Vector3 position = fieldCenter + Vector3((unit(mRandom) * 2.0f - 1.0f) * half, 0.0f, (unit(mRandom) * 2.0f - 1.0f) * half);

		Transform* transform = driver.entity->getTransform();
		transform->setPosition(position);
		transform->setRotation(Vector3(0.0f, unit(mRandom) * 6.2831853f, 0.0f));

		driver.body->setVelocity(Vector3());
		driver.body->setAngularVelocity(Vector3());
		driver.controllable->resetControl();
	}

	driver.body->takeCollisions();

	// Приклади, горизонт яких не встиг минути, лишаються без мітки й відкидаються
	driver.pending.clear();
	driver.hasState = false;
	driver.steps = 0;
	driver.slowSteps = 0;
	driver.targetTimer = 0;
	std::fill(driver.actions.begin(), driver.actions.end(), 0.0f);

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

			// Траєкторію, яку зараз записує інший об'єкт, не віддаємо: інакше вона змішала б два епізоди
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

// Додає перехід до навчальних або перевірних даних
void DynamicsTrainer::storeTransition(const Driver& driver, const float* action, const float* next)
{
	int stride = mModel.getTransitionSize();
	std::vector<float> row(stride);

	std::copy(driver.state.begin(), driver.state.end(), row.begin());
	std::copy(action, action + mActionSize, row.begin() + mStateSize);
	std::copy(next, next + mStateSize, row.begin() + mStateSize + mActionSize);

	mCollected++;

	if (driver.trajectory >= 0)
	{
		// Точка перевірної траєкторії: стан і дія на кроці та поза на його початку
		std::vector<float>& trajectory = mTrajectories[driver.trajectory];
		trajectory.insert(trajectory.end(), row.begin(), row.begin() + mStateSize + mActionSize);
		trajectory.push_back(driver.x);
		trajectory.push_back(driver.z);
		trajectory.push_back(driver.heading);

		storeRow(mValidationData, mValidationNext, VALIDATION_CAPACITY, row.data(), stride);
		return;
	}

	storeRow(mTrainData, mTrainNext, (size_t)std::max(batchSize, datasetCapacity), row.data(), stride);
}

// Додає приклад з відомою міткою до навчальних або перевірних даних моделі зіткнень
void DynamicsTrainer::storeCollision(const Driver& driver, PendingCollision& pending, float label)
{
	int rays = mModel.getCollisionRays();
	int count = std::max(1, pending.age);

	// Дія прикладу - середня за час від його створення: вона й привела або не привела до удару
	for (int i = 0; i < mActionSize; i++) pending.row[rays + mStateSize + i] /= (float)count;

	pending.row.back() = label;

	bool validation = driver.trajectory >= 0;
	std::vector<float>& data = validation ? mCollisionValidation : mCollisionData;
	size_t& next = validation ? mCollisionValidationNext : mCollisionNext;
	size_t capacity = validation ? VALIDATION_CAPACITY : (size_t)std::max(batchSize, datasetCapacity);

	storeRow(data, next, capacity, pending.row.data(), mModel.getCollisionSampleSize());

	if (!validation)
	{
		mCollisionTotal++;
		if (label > 0.5f) mCollisionPositives++;
	}
}

// Оновлює приклади, що чекають на мітку: удар позначає всі, а спливлий горизонт - найстаріші як безпечні
void DynamicsTrainer::updatePending(Driver& driver, const float* action, bool collided)
{
	int offset = mModel.getCollisionRays() + mStateSize;

	for (PendingCollision& pending : driver.pending)
	{
		for (int i = 0; i < mActionSize; i++) pending.row[offset + i] += action[i];

		pending.age++;
	}

	// Удар стався в межах горизонту кожного прикладу, що ще чекає
	if (collided)
	{
		for (PendingCollision& pending : driver.pending) storeCollision(driver, pending, 1.0f);

		driver.pending.clear();
		return;
	}

	// Приклади, горизонт яких минув без удару, - безпечні
	size_t done = 0;

	while (done < driver.pending.size() && driver.pending[done].age >= mModel.getCollisionHorizon())
	{
		storeCollision(driver, driver.pending[done], 0.0f);
		done++;
	}

	driver.pending.erase(driver.pending.begin(), driver.pending.begin() + done);
}

// Записує переходи всіх об'єктів і задає випадкові дії
void DynamicsTrainer::fixedUpdate()
{
	if (mFailed) return;

	if (!mInitialized)
	{
		if (!initialize()) mFailed = true;
		return;
	}

	std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
	bool collisions = mModel.hasCollisionModel();

	std::vector<float> state(mStateSize);
	std::vector<float> action(mActionSize);

	for (Driver& driver : mDrivers)
	{
		if (!EntityManager::get()->isAlive(driver.entity))
		{
			std::cout << "DynamicsTrainer: a driver was destroyed, data collection stopped" << std::endl;
			mFailed = true;
			return;
		}

		// Об'єкт може збиратися на першому кроці, і фізичного тіла ще немає; до того записувати нічого
		if (!driver.controllable->getBody()) continue;

		bool collided = driver.body->takeCollisions() > 0;
		bool upright = driver.controllable->isUpright();

		float x, z, heading;
		driver.controllable->getState(state.data());
		driver.controllable->getPlanarPose(x, z, heading);

		// Дія, з якою об'єкт пройшов попередній крок, - та, що зараз стоїть у керуванні
		driver.controllable->getActions(action.data());

		// Удари й перевертання модель не описує, тож такі переходи не записуються, а епізод завершується
		bool broken = collided || !upright;

		if (driver.hasState && !broken) storeTransition(driver, action.data(), state.data());

		// Удар дає приклади моделі зіткнень; перевертання без удару - ні, бо що до нього призвело, невідомо
		if (collisions && (collided || upright)) updatePending(driver, action.data(), collided);

		driver.steps++;
		driver.slowSteps = std::fabs(state[0]) < 0.3f ? driver.slowSteps + 1 : 0;

		bool onField = !driver.policy && !driver.inEnvironment;
		bool outside = onField && (std::fabs(x - fieldCenter.x) > fieldSize * 0.5f || std::fabs(z - fieldCenter.z) > fieldSize * 0.5f);
		bool stuck = driver.slowSteps * SimulationSettings::fixedStep > 3.0f;
		bool finished = driver.policy && driver.agent->isEpisodeDone();

		if (broken || outside || stuck || finished || driver.steps >= episodeSteps)
		{
			respawn(driver);
			continue;
		}

		driver.state = state;
		driver.x = x;
		driver.z = z;
		driver.heading = heading;
		driver.hasState = true;

		// Новий приклад для моделі зіткнень: огляд і стан зараз, а мітку визначить найближчий горизонт
		if (collisions)
		{
			int rays = mModel.getCollisionRays();

			PendingCollision pending;
			pending.row.assign(mModel.getCollisionSampleSize(), 0.0f);
			DynamicsModel::scanSurroundings(driver.controllable, mModel, pending.row.data());
			std::copy(state.begin(), state.end(), pending.row.begin() + rays);
			driver.pending.push_back(std::move(pending));
		}

		// Випадкові дії плавно тягнуться до цілей, що час від часу змінюються
		if (!driver.policy)
		{
			if (--driver.targetTimer <= 0)
			{
				driver.controllable->sampleExplorationActions(mRandom, driver.targets.data());
				driver.targetTimer = std::uniform_int_distribution<int>(20, 120)(mRandom);
			}

			for (int i = 0; i < mActionSize; i++)
			{
				float value = driver.actions[i] + (driver.targets[i] - driver.actions[i]) * 0.08f + noise(mRandom) * 0.04f;
				driver.actions[i] = std::max(-1.0f, std::min(value, 1.0f));
			}

			driver.controllable->setActions(driver.actions.data());
		}
	}
}

// Навчає мережу, доки не вичерпано бюджет кадру
void DynamicsTrainer::train()
{
	int stride = mModel.getTransitionSize();
	size_t samples = mTrainData.size() / stride;

	if ((int)samples < std::max(warmupSamples, batchSize)) return;

	// Нормування береться з першої зібраної вибірки й далі не змінюється, бо мережа навчається саме під нього
	if (!mModel.hasNormalization()) mModel.computeNormalization(mTrainData);

	mOptimizer.learningRate = learningRate;

	int collisionStride = mModel.getCollisionSampleSize();
	size_t collisionSamples = mModel.hasCollisionModel() ? mCollisionData.size() / collisionStride : 0;

	double start = nowSeconds();
	std::uniform_int_distribution<size_t> pick(0, samples - 1);

	do
	{
		mBatch.resize(batchSize);

		for (int i = 0; i < batchSize; i++) mBatch[i] = mTrainData.data() + pick(mRandom) * stride;

		float loss = mModel.trainBatch(mBatch.data(), batchSize, mOptimizer);

		mTrainLoss = mBatches == 0 ? loss : mTrainLoss * 0.98f + loss * 0.02f;
		mBatches++;

		// Модель зіткнень навчається поруч, щойно серед прикладів з'явилися удари
		if ((int)collisionSamples >= batchSize && mCollisionPositives > 0)
		{
			mCollisionOptimizer.learningRate = learningRate;

			std::uniform_int_distribution<size_t> pickCollision(0, collisionSamples - 1);

			for (int i = 0; i < batchSize; i++) mBatch[i] = mCollisionData.data() + pickCollision(mRandom) * collisionStride;

			float collisionLoss = mModel.trainCollisionBatch(mBatch.data(), batchSize, mCollisionOptimizer);

			mCollisionTrainLoss = mCollisionBatches == 0 ? collisionLoss : mCollisionTrainLoss * 0.98f + collisionLoss * 0.02f;
			mCollisionBatches++;
		}
	} while ((nowSeconds() - start) * 1000.0 < trainMilliseconds);
}

// Повертає середню похибку положення на горизонті H на перевірних траєкторіях
float DynamicsTrainer::horizonError(int horizon, int segments)
{
	// Точка траєкторії: стан, дія, положення x і z та курс
	int stride = mStateSize + mActionSize + 3;
	int pose = mStateSize + mActionSize;

	std::vector<int> usable;

	for (size_t i = 0; i < mTrajectories.size(); i++)
	{
		if ((int)(mTrajectories[i].size() / stride) > horizon) usable.push_back((int)i);
	}

	if (usable.empty()) return -1.0f;

	// Випадкові відрізки траєкторій прогнозуються разом, одним пакетом на крок
	std::vector<const std::vector<float>*> chosen(segments);
	std::vector<int> starts(segments);
	std::vector<float> states((size_t)segments * mStateSize), next((size_t)segments * mStateSize), actions((size_t)segments * mActionSize);
	std::vector<float> xs(segments), zs(segments), headings(segments);

	for (int n = 0; n < segments; n++)
	{
		chosen[n] = &mTrajectories[usable[std::uniform_int_distribution<int>(0, (int)usable.size() - 1)(mRandom)]];
		starts[n] = std::uniform_int_distribution<int>(0, (int)(chosen[n]->size() / stride) - horizon - 1)(mRandom);

		const float* first = chosen[n]->data() + (size_t)starts[n] * stride;
		std::copy(first, first + mStateSize, states.data() + (size_t)n * mStateSize);
		xs[n] = first[pose];
		zs[n] = first[pose + 1];
		headings[n] = first[pose + 2];
	}

	// Прогноз іде з відомими діями, але без поправок від справжнього стану: похибка накопичується так само, як під час планування
	double total = 0.0;

	for (int h = 1; h <= horizon; h++)
	{
		for (int n = 0; n < segments; n++)
		{
			const float* applied = chosen[n]->data() + (size_t)(starts[n] + h - 1) * stride;
			std::copy(applied + mStateSize, applied + mStateSize + mActionSize, actions.data() + (size_t)n * mActionSize);
		}

		mModel.predictBatch(states.data(), actions.data(), segments, next.data());

		for (int n = 0; n < segments; n++)
		{
			LearnedDynamics::integratePose(xs[n], zs[n], headings[n], next.data() + (size_t)n * mStateSize, SimulationSettings::fixedStep);

			const float* actual = chosen[n]->data() + (size_t)(starts[n] + h) * stride;
			float dx = xs[n] - actual[pose];
			float dz = zs[n] - actual[pose + 1];
			total += std::sqrt(dx * dx + dz * dz);
		}

		states.swap(next);
	}

	return (float)(total / ((double)segments * horizon));
}

// Обчислює похибку одного кроку та положення на горизонті на перевірних даних і зберігає модель, якщо вона покращилася
void DynamicsTrainer::evaluate()
{
	mLastEvaluation = nowSeconds();

	int stride = mModel.getTransitionSize();
	size_t validationSamples = mValidationData.size() / stride;

	if (mBatches == 0 || validationSamples == 0) return;

	int count = std::min((int)validationSamples, VALIDATION_SAMPLE);
	std::uniform_int_distribution<size_t> pick(0, validationSamples - 1);

	mBatch.resize(count);

	for (int i = 0; i < count; i++) mBatch[i] = mValidationData.data() + pick(mRandom) * stride;

	float validation = mModel.evaluate(mBatch.data(), count);
	float horizon = horizonError(std::max(1, evaluationHorizon), HORIZON_SEGMENTS);

	mValidationHistory.push_back(validation);

	// Модель зіткнень перевіряється на прикладах з перевірних епізодів
	bool collisionImproved = false;
	int collisionStride = mModel.getCollisionSampleSize();
	size_t collisionSamples = mModel.hasCollisionModel() ? mCollisionValidation.size() / collisionStride : 0;

	if (mCollisionBatches > 0 && collisionSamples > 0)
	{
		int collisionCount = std::min((int)collisionSamples, VALIDATION_SAMPLE);
		std::uniform_int_distribution<size_t> pickCollision(0, collisionSamples - 1);

		mBatch.resize(collisionCount);

		for (int i = 0; i < collisionCount; i++) mBatch[i] = mCollisionValidation.data() + pickCollision(mRandom) * collisionStride;

		float collisionLoss = 0.0f;
		mModel.evaluateCollision(mBatch.data(), collisionCount, collisionLoss, mCollisionRecall, mCollisionPrecision);
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
		ImGui::TextWrapped("Training is not running: the agent prefab needs a RigidBody and a component derived from ControllableBody (see the console).");
		ImGui::End();
		return;
	}

	int stride = std::max(1, mModel.getTransitionSize());
	int trainSamples = (int)(mTrainData.size() / stride);

	ImGui::Text("Transitions: %d train, %d validation, %d trajectories", trainSamples, (int)(mValidationData.size() / stride), (int)mTrajectories.size());

	if (trainSamples < warmupSamples)
	{
		ImGui::Text("Collecting data before training: %d / %d", trainSamples, warmupSamples);
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

	if (mModel.hasCollisionModel())
	{
		ImGui::Separator();
		ImGui::Text("Collision samples: %lld (%.1f%% hits), %d validation", mCollisionTotal, mCollisionTotal > 0 ? 100.0 * mCollisionPositives / mCollisionTotal : 0.0, (int)(mCollisionValidation.size() / mModel.getCollisionSampleSize()));

		if (!mCollisionLossHistory.empty())
		{
			float high = *std::max_element(mCollisionLossHistory.begin(), mCollisionLossHistory.end());

			ImGui::Text("Collision loss: %.4f, hits caught: %.0f%%, alarms right: %.0f%%", mCollisionLossHistory.back(), mCollisionRecall * 100.0f, mCollisionPrecision * 100.0f);
			ImGui::PlotLines("Collision", mCollisionLossHistory.data(), (int)mCollisionLossHistory.size(), 0, nullptr, 0.0f, high, ImVec2(0, 60));
		}
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
	visitor.reference("environment", environment);
	visitor.property("policyShare", policyShare, 0.01f);
	visitor.property("environmentShare", environmentShare, 0.01f);
	visitor.property("bodyCount", bodyCount);
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
	visitor.property("learnCollisions", learnCollisions);
	visitor.property("collisionRays", collisionRays);
	visitor.property("collisionRange", collisionRange, 0.5f);
	visitor.property("collisionHorizon", collisionHorizon);
	visitor.property("continueTraining", continueTraining);
	visitor.property("collisionGroup", collisionGroup);
	visitor.property("seed", seed);
	visitor.property("stepsPerFrame", stepsPerFrame);
	visitor.property("renderScene", renderScene);
	visitor.asset("modelAsset", modelAsset, LearnedDynamics::FILE_EXTENSION);

	bodyCount = std::max(1, bodyCount);
	episodeSteps = std::max(10, episodeSteps);
	datasetCapacity = std::max(1000, datasetCapacity);
	warmupSamples = std::max(0, warmupSamples);
	batchSize = std::max(1, std::min(batchSize, 4096));
	evaluationHorizon = std::max(1, evaluationHorizon);
	trainMilliseconds = std::max(0.0f, trainMilliseconds);
	collisionRays = std::max(1, std::min(collisionRays, 256));
	collisionRange = std::max(0.5f, collisionRange);
	collisionHorizon = std::max(1, collisionHorizon);
	stepsPerFrame = std::max(1, std::min(stepsPerFrame, 200));
}
