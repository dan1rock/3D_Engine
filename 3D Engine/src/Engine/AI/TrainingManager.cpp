#include "TrainingManager.h"
#include "NeuroevolutionBrain.h"
#include "VehicleController.h"
#include "RaySensor.h"
#include "AgentSensor.h"
#include "NeuralNetwork.h"
#include "Prefab.h"
#include "RigidBody.h"
#include "EntityManager.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <string>

// Повертає довжину ламаної в горизонтальній площині від точки start до кінця
static float routeLength(const std::vector<Vector3>& waypoints, int start)
{
	float length = 0.0f;

	for (size_t i = std::max(start, 0) + 1; i < waypoints.size(); i++)
	{
		float dx = waypoints[i].x - waypoints[i - 1].x;
		float dz = waypoints[i].z - waypoints[i - 1].z;
		length += std::sqrt(dx * dx + dz * dz);
	}

	return length;
}

// Повертає симуляції звичайний крок і малювання сцени
TrainingManager::~TrainingManager()
{
	SimulationSettings::fixedStepsPerFrame = 0;
	SimulationSettings::renderScene = true;
}

// Передає симуляції темп і малювання та показує вікно з ходом навчання
void TrainingManager::update()
{
	SimulationSettings::fixedStepsPerFrame = stepsPerFrame > 1 ? stepsPerFrame : 0;
	SimulationSettings::renderScene = renderScene;

	drawWindow();
}

// Керує епізодами: зупиняє агентів, що завершили епізод, і переходить до наступного епізоду чи покоління
void TrainingManager::fixedUpdate()
{
	if (mFailed) return;

	if (!mInitialized)
	{
		if (!initialize())
		{
			mFailed = true;
			return;
		}

		startEpisode();
		return;
	}

	// Агента могли видалити з редактора посеред гри: далі вказівники на нього недійсні
	for (Entity* agent : mAgents)
	{
		if (!EntityManager::get()->isAlive(agent))
		{
			std::cout << "TrainingManager: an agent was destroyed, training stopped" << std::endl;
			mFailed = true;
			return;
		}
	}

	if (mValidator && !EntityManager::get()->isAlive(mValidator))
	{
		mValidator = nullptr;
		mValidatorBrain = nullptr;
	}

	updateValidation();

	mTotalSteps++;
	mEpisodeStep++;

	bool allDone = true;

	for (size_t i = 0; i < mAgents.size(); i++)
	{
		if (!mBrains[i]->isEpisodeDone())
		{
			allDone = false;
			continue;
		}

		// Агент, що завершив епізод, виходить із симуляції й не витрачає часу фізики до наступного епізоду
		if (mAgents[i]->isActiveSelf) mAgents[i]->isActiveSelf = false;
	}

	// Запас понад ліміт кроків на випадок, коли мозок агента з якоїсь причини не завершив епізод сам
	if (!allDone && mEpisodeStep < maxEpisodeSteps + 50) return;

	for (size_t i = 0; i < mAgents.size(); i++)
	{
		mEpisodeFitness[i] += mBrains[i]->getEpisodeReward();
	}

	mEpisode++;

	if (mEpisode >= (int)mPlans.size())
	{
		finishGeneration();
	}

	if (!mFailed) startEpisode();
}

// Створює популяцію й агентів; повертає false, якщо навчання неможливо налаштувати
bool TrainingManager::initialize()
{
	mInitialized = true;

	AgentSensor::collectRoutes(route, mRoutes);
	AgentSensor::collectRoutes(validationRoute, mValidationRoutes);

	if (!agentPrefab || mRoutes.empty() || mRoutes.front()->getChildren()->size() < 2)
	{
		std::cout << "TrainingManager: needs an agent prefab and a route with at least two waypoints" << std::endl;
		return false;
	}

	populationSize = std::max(1, populationSize);

	NeuralNetwork network(NeuroevolutionBrain::defaultLayers());

	unsigned int actualSeed = seed != 0 ? (unsigned int)seed : std::random_device()();
	mAlgorithm.init(populationSize, network.getParameterCount(), actualSeed);

	// Продовження навчання: перша особина - збережені ваги, решта - їхні мутанти
	NeuralNetwork saved;
	bool resume = continueTraining && saved.load(weightsAsset) && saved.getLayers() == network.getLayers();

	std::normal_distribution<float> normal(0.0f, 1.0f);

	for (int i = 0; i < populationSize; i++)
	{
		std::vector<float>& individual = mAlgorithm.getIndividual(i);

		if (resume)
		{
			individual = saved.getParameters();

			if (i > 0)
			{
				for (float& value : individual) value += mutationSigma * normal(mAlgorithm.getRandom());
			}
		}
		else
		{
			network.randomize(mAlgorithm.getRandom());
			individual = network.getParameters();
		}
	}

	for (int i = 0; i < populationSize; i++)
	{
		NeuroevolutionBrain* brain = nullptr;
		Entity* agent = createAgent("Agent " + std::to_string(i), brain);

		if (!agent) return false;

		mAgents.push_back(agent);
		mBrains.push_back(brain);
	}

	// Перевірний агент живе окремо від популяції й не впливає на відбір
	if (!mValidationRoutes.empty())
	{
		mValidator = createAgent("Validation Agent", mValidatorBrain);

		if (mValidator)
		{
			// Перевірні маршрути проходяться повністю, тож агент отримує вдвічі більше часу
			mValidatorBrain->maxEpisodeSteps = maxEpisodeSteps * 2;
			mValidator->isActiveSelf = false;
		}
	}

	mEpisodeFitness.assign(populationSize, 0.0f);
	mEpisode = 0;
	planEpisodes();

	return true;
}

// Створює агента з образу та налаштовує його для навчання; nullptr, якщо образ не підходить
Entity* TrainingManager::createAgent(const std::string& name, NeuroevolutionBrain*& brain)
{
	Entity* agent = agentPrefab->instantiate();
	agent->setName(name);

	VehicleController* vehicle = agent->getComponent<VehicleController>();
	RigidBody* body = agent->getComponent<RigidBody>();

	if (!vehicle || !body)
	{
		std::cout << "TrainingManager: agent prefab needs a RigidBody and a VehicleController" << std::endl;
		agent->destroy();
		return nullptr;
	}

	// Бракує сенсорів чи мозку - вони додаються з налаштуваннями за замовчуванням
	if (!agent->getComponent<RaySensor>()) agent->addComponent<RaySensor>();
	if (!agent->getComponent<AgentSensor>()) agent->addComponent<AgentSensor>();

	brain = agent->getComponent<NeuroevolutionBrain>();
	if (!brain) brain = agent->addComponent<NeuroevolutionBrain>();

	vehicle->playerInput = false;
	vehicle->debugWindow = false;
	brain->episodic = true;
	brain->maxEpisodeSteps = maxEpisodeSteps;
	body->setCollisionGroup((unsigned int)std::max(0, collisionGroup));
	body->setReportCollisions(true);

	return agent;
}

// Ставить агента на маршрут і починає йому новий епізод
void TrainingManager::placeAgent(Entity* agent, NeuroevolutionBrain* brain, Entity* agentRoute, int start, float offset, float angle)
{
	std::vector<Vector3> waypoints;
	AgentSensor::collectWaypoints(agentRoute, waypoints);

	start = std::max(0, std::min(start, (int)waypoints.size() - 2));

	Vector3 direction = waypoints[start + 1] - waypoints[start];
	direction.y = 0.0f;
	direction.normalize();

	// Зсув убік рахується від напряму маршруту: праворуч від нього при додатному значенні
	Vector3 right(direction.z, 0.0f, -direction.x);
	Vector3 position = waypoints[start] + right * offset + Vector3(0.0f, spawnHeight, 0.0f);

	RigidBody* body = agent->getComponent<RigidBody>();

	// Актор має повернутися в сцену до того, як йому задаватимуть швидкість
	agent->isActiveSelf = true;
	body->syncActivity();

	Transform* transform = agent->getTransform();
	transform->setPosition(position);
	transform->setRotation(Vector3(0.0f, std::atan2(direction.x, direction.z) + angle, 0.0f));

	body->setVelocity(Vector3());
	body->setAngularVelocity(Vector3());

	AgentSensor* sensor = agent->getComponent<AgentSensor>();
	sensor->route = agentRoute;
	sensor->resetRoute(start);

	agent->getComponent<VehicleController>()->resetState();
	brain->beginEpisode();
}

// Ставить усіх агентів популяції на старт поточного епізоду
void TrainingManager::startEpisode()
{
	const EpisodePlan& plan = mPlans[mEpisode];

	if (!EntityManager::get()->isAlive(plan.route) || plan.route->getChildren()->size() < 2)
	{
		std::cout << "TrainingManager: a training route lost its waypoints, training stopped" << std::endl;
		mFailed = true;
		return;
	}

	for (size_t i = 0; i < mAgents.size(); i++)
	{
		// Параметри особини підставляються на першому епізоді покоління, а після навчання всі показують найкращу
		if (mEpisode == 0)
		{
			mBrains[i]->setParameters(mFinished ? mAlgorithm.getBest() : mAlgorithm.getIndividual((int)i));
		}

		placeAgent(mAgents[i], mBrains[i], plan.route, plan.start, plan.offset, plan.angle);
	}

	mEpisodeStep = 0;
}

// Оцінює покоління, зберігає найкращу особину, якщо перевірки немає, і створює наступне покоління
void TrainingManager::finishGeneration()
{
	mEpisode = 0;

	// Після навчання агенти лише показують найкращу особину, а покоління більше не змінюються
	if (mFinished)
	{
		std::fill(mEpisodeFitness.begin(), mEpisodeFitness.end(), 0.0f);
		planEpisodes();
		return;
	}

	int episodes = std::max(1, (int)mPlans.size());

	for (int i = 0; i < mAlgorithm.getPopulationSize(); i++)
	{
		mAlgorithm.setFitness(i, mEpisodeFitness[i] / (float)episodes);
	}

	int generation = mAlgorithm.getGeneration();

	mAlgorithm.evolve(eliteCount, tournamentSize, currentSigma());

	mBestHistory.push_back(mAlgorithm.getBestFitness());
	mMeanHistory.push_back(mAlgorithm.getMeanFitness());

	// Без перевірних маршрутів зберігається найкраща особина кожного покоління
	if (!mValidator)
	{
		NeuralNetwork best(NeuroevolutionBrain::defaultLayers());
		best.setParameters(mAlgorithm.getBest());
		mSaved = best.save(weightsAsset, generation + 1, mAlgorithm.getBestFitness());
	}

	std::cout << "Generation " << generation + 1 << ": best " << mAlgorithm.getBestFitness() << ", mean " << mAlgorithm.getMeanFitness() << std::endl;

	if (mAlgorithm.getGeneration() >= generations) mFinished = true;

	std::fill(mEpisodeFitness.begin(), mEpisodeFitness.end(), 0.0f);
	planEpisodes();
}

// Складає початкові умови епізодів наступного покоління
void TrainingManager::planEpisodes()
{
	std::mt19937& random = mAlgorithm.getRandom();
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);

	int episodes = std::max(1, episodesPerGeneration);

	// Маршрути перемішуються, тож кожне покоління бачить їх у різному порядку, а за достатньої кількості епізодів - усі
	std::vector<int> order(mRoutes.size());

	for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;

	std::shuffle(order.begin(), order.end(), random);

	mPlans.resize(episodes);

	for (int e = 0; e < episodes; e++)
	{
		EpisodePlan& plan = mPlans[e];
		plan.route = mRoutes[order[e % order.size()]];

		std::vector<Vector3> waypoints;
		AgentSensor::collectWaypoints(plan.route, waypoints);

		// Старт вибирається серед точок, після яких лишається достатня частина маршруту
		float total = routeLength(waypoints, 0);
		std::vector<int> candidates;

		for (int i = 0; i + 1 < (int)waypoints.size(); i++)
		{
			if (routeLength(waypoints, i) >= minStartRemaining * total) candidates.push_back(i);
		}

		if (candidates.empty()) candidates.push_back(0);

		plan.start = candidates[std::uniform_int_distribution<int>(0, (int)candidates.size() - 1)(random)];
		plan.offset = unit(random) * startOffset;
		plan.angle = unit(random) * startAngle * 3.14159265f / 180.0f;
	}
}

// Веде перевірку найкращої особини на перевірних маршрутах і зберігає мережу, що пройшла їх найкраще
void TrainingManager::updateValidation()
{
	if (!mValidator) return;

	// Перевірка йде: чекаємо, поки агент завершить поточний маршрут
	if (mValidationGeneration >= 0)
	{
		if (!mValidatorBrain->isEpisodeDone()) return;

		AgentSensor* sensor = mValidator->getComponent<AgentSensor>();

		std::vector<Vector3> waypoints;
		AgentSensor::collectWaypoints(sensor->route, waypoints);

		float total = routeLength(waypoints, 0);
		float completion = mValidatorBrain->getEpisodeEnd() == EpisodeEnd::Goal ? 1.0f : (total > 0.0f ? 1.0f - sensor->getRemainingDistance() / total : 0.0f);

		mValidationCompletion += std::max(0.0f, std::min(completion, 1.0f));
		mValidationReward += mValidatorBrain->getEpisodeReward();
		mValidationRouteIndex++;

		// Наступний перевірний маршрут тієї самої особини
		if (mValidationRouteIndex < (int)mValidationRoutes.size())
		{
			placeAgent(mValidator, mValidatorBrain, mValidationRoutes[mValidationRouteIndex], 0, 0.0f, 0.0f);
			return;
		}

		float routes = (float)mValidationRoutes.size();
		float average = mValidationCompletion / routes;
		float reward = mValidationReward / routes;

		mValidationHistory.push_back(average * 100.0f);

		// Зберігається мережа, що пройшла найбільшу частку перевірних маршрутів, а за рівної частки - з більшою винагородою
		bool better = average > mBestValidation + 0.001f || (std::fabs(average - mBestValidation) <= 0.001f && reward > mBestValidationReward);

		if (better)
		{
			mBestValidation = average;
			mBestValidationReward = reward;
			mBestValidationGeneration = mValidationGeneration + 1;

			NeuralNetwork network(NeuroevolutionBrain::defaultLayers());
			network.setParameters(mValidationParameters);
			mSaved = network.save(weightsAsset, mBestValidationGeneration, reward);
		}

		std::cout << "Validation of generation " << mValidationGeneration + 1 << ": " << average * 100.0f << "% of the route" << (better ? ", saved" : "") << std::endl;

		mValidatedGeneration = mValidationGeneration;
		mValidationGeneration = -1;
		mValidator->isActiveSelf = false;
	}

	// Нова перевірка починається, щойно з'явилася найкраща особина ще не перевіреного покоління
	int latest = mAlgorithm.getGeneration() - 1;

	if (latest < 0 || latest == mValidatedGeneration) return;

	mValidationParameters = mAlgorithm.getBest();
	mValidationGeneration = latest;
	mValidationRouteIndex = 0;
	mValidationCompletion = 0.0f;
	mValidationReward = 0.0f;

	mValidatorBrain->setParameters(mValidationParameters);
	placeAgent(mValidator, mValidatorBrain, mValidationRoutes[0], 0, 0.0f, 0.0f);
}

// Повертає крок мутації для поточного покоління
float TrainingManager::currentSigma() const
{
	float progress = generations > 1 ? (float)mAlgorithm.getGeneration() / (float)(generations - 1) : 1.0f;
	progress = std::max(0.0f, std::min(progress, 1.0f));

	return mutationSigma * (1.0f - (1.0f - finalSigmaRatio) * progress);
}

// Малює вікно з ходом навчання
void TrainingManager::drawWindow()
{
	ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_FirstUseEver);
	ImGui::Begin("Neuroevolution");

	if (mFailed)
	{
		ImGui::TextWrapped("Training is not running: check the agent prefab and the routes in the inspector.");
		ImGui::End();
		return;
	}

	int alive = 0;

	for (NeuroevolutionBrain* brain : mBrains)
	{
		if (!brain->isEpisodeDone()) alive++;
	}

	ImGui::Text("Generation %d / %d%s", mAlgorithm.getGeneration() + (mFinished ? 0 : 1), generations, mFinished ? " (finished)" : "");

	if (!mPlans.empty() && mEpisode < (int)mPlans.size())
	{
		ImGui::Text("Episode %d / %d on %s, step %d", mEpisode + 1, (int)mPlans.size(), mPlans[mEpisode].route->getName().c_str(), mEpisodeStep);
	}

	ImGui::Text("Agents running: %d / %d", alive, (int)mAgents.size());
	ImGui::Text("Mutation sigma: %.4f", currentSigma());
	ImGui::Text("Simulated time: %.0f s", (double)mTotalSteps * SimulationSettings::fixedStep);

	if (!mBestHistory.empty())
	{
		ImGui::Text("Best fitness: %.2f, mean: %.2f", mBestHistory.back(), mMeanHistory.back());

		float low = *std::min_element(mMeanHistory.begin(), mMeanHistory.end());
		float high = *std::max_element(mBestHistory.begin(), mBestHistory.end());

		ImGui::PlotLines("Best", mBestHistory.data(), (int)mBestHistory.size(), 0, nullptr, low, high, ImVec2(0, 60));
		ImGui::PlotLines("Mean", mMeanHistory.data(), (int)mMeanHistory.size(), 0, nullptr, low, high, ImVec2(0, 60));
	}

	if (mValidator)
	{
		ImGui::Separator();

		if (mBestValidation >= 0.0f)
		{
			ImGui::Text("Validation best: %.0f%% of the route (generation %d)", mBestValidation * 100.0f, mBestValidationGeneration);
		}
		else
		{
			ImGui::TextDisabled("Validation starts after the first generation");
		}

		if (!mValidationHistory.empty())
		{
			ImGui::PlotLines("Validation %", mValidationHistory.data(), (int)mValidationHistory.size(), 0, nullptr, 0.0f, 100.0f, ImVec2(0, 60));
		}
	}

	if (mSaved) ImGui::Text("Best weights saved to %s", weightsAsset.c_str());

	ImGui::SliderInt("Steps per frame", &stepsPerFrame, 1, 50);
	ImGui::Checkbox("Render scene", &renderScene);

	ImGui::End();
}

// Перелічує власні поля для файлу сцени та інспектора
void TrainingManager::visitProperties(PropertyVisitor& visitor)
{
	visitor.reference("agentPrefab", agentPrefab);
	visitor.reference("route", route);
	visitor.reference("validationRoute", validationRoute);
	visitor.property("populationSize", populationSize);
	visitor.property("eliteCount", eliteCount);
	visitor.property("tournamentSize", tournamentSize);
	visitor.property("mutationSigma", mutationSigma, 0.001f);
	visitor.property("finalSigmaRatio", finalSigmaRatio, 0.01f);
	visitor.property("generations", generations);
	visitor.property("episodesPerGeneration", episodesPerGeneration);
	visitor.property("maxEpisodeSteps", maxEpisodeSteps);
	visitor.property("spawnHeight", spawnHeight, 0.05f);
	visitor.property("startOffset", startOffset, 0.05f);
	visitor.property("startAngle", startAngle, 0.5f);
	visitor.property("minStartRemaining", minStartRemaining, 0.01f);
	visitor.property("collisionGroup", collisionGroup);
	visitor.property("seed", seed);
	visitor.property("continueTraining", continueTraining);
	visitor.property("stepsPerFrame", stepsPerFrame);
	visitor.property("renderScene", renderScene);
	visitor.property("weightsAsset", weightsAsset);

	// Популяцію вже створено, тож її розмір і решта меж застосовуються лише до нового навчання
	populationSize = std::max(1, populationSize);
	eliteCount = std::max(0, eliteCount);
	tournamentSize = std::max(1, tournamentSize);
	generations = std::max(1, generations);
	episodesPerGeneration = std::max(1, episodesPerGeneration);
	maxEpisodeSteps = std::max(1, maxEpisodeSteps);
	minStartRemaining = std::max(0.0f, std::min(minStartRemaining, 1.0f));
	stepsPerFrame = std::max(1, std::min(stepsPerFrame, 200));
}
