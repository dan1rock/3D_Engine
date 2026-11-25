#include "TrainingManager.h"
#include "Agent.h"
#include "AgentBrain.h"
#include "NeuroevolutionBrain.h"
#include "ControllableBody.h"
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
	for (const Member& member : mMembers)
	{
		if (!EntityManager::get()->isAlive(member.entity))
		{
			std::cout << "TrainingManager: an agent was destroyed, training stopped" << std::endl;
			mFailed = true;
			return;
		}
	}

	if (mValidator.entity && !EntityManager::get()->isAlive(mValidator.entity)) mValidator = Member();

	updateValidation();

	mTotalSteps++;
	mEpisodeStep++;

	bool allDone = true;

	for (Member& member : mMembers)
	{
		if (!member.agent->isEpisodeDone())
		{
			allDone = false;
			continue;
		}

		// Агент, що завершив епізод, виходить із симуляції й не витрачає часу фізики до наступного епізоду
		if (member.entity->isActiveSelf) member.entity->isActiveSelf = false;
	}

	// Запас понад ліміт кроків на випадок, коли агент з якоїсь причини не завершив епізод сам
	if (!allDone && mEpisodeStep < maxEpisodeSteps + 50) return;

	for (size_t i = 0; i < mMembers.size(); i++)
	{
		mEpisodeFitness[i] += mMembers[i].agent->getEpisodeReward();
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

	if (!agentPrefab)
	{
		std::cout << "TrainingManager: needs an agent prefab" << std::endl;
		return false;
	}

	populationSize = std::max(1, populationSize);

	for (int i = 0; i < populationSize; i++)
	{
		Member member;

		if (!createMember("Agent " + std::to_string(i), member)) return false;

		mMembers.push_back(member);
	}

	// Розміри мережі задають спостереження й дії агента та приховані шари його мозку
	mLayers = mMembers.front().brain->getLayers(*mMembers.front().agent);

	if (mLayers.front() <= 0 || mLayers.back() <= 0)
	{
		std::cout << "TrainingManager: the agent has no observations or actions" << std::endl;
		return false;
	}

	mVariants = std::max(1, mMembers.front().agent->getVariantCount(environment));

	NeuralNetwork network(mLayers);

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

	// Перевірний агент живе окремо від популяції й не впливає на відбір
	if (validationEnvironment && createMember("Validation Agent", mValidator))
	{
		mValidationVariants = std::max(1, mValidator.agent->getVariantCount(validationEnvironment));

		// Перевірні епізоди проходяться повністю, тож агент отримує вдвічі більше часу
		mValidator.agent->maxEpisodeSteps = maxEpisodeSteps * 2;
		mValidator.entity->isActiveSelf = false;
	}

	mEpisodeFitness.assign(populationSize, 0.0f);
	mEpisode = 0;
	planEpisodes();

	return true;
}

// Створює агента з образу та налаштовує його для навчання; false, якщо образ не підходить
bool TrainingManager::createMember(const std::string& name, Member& member)
{
	Entity* entity = agentPrefab->instantiate();
	entity->setName(name);

	Agent* agent = entity->getComponent<Agent>();

	if (!agent)
	{
		std::cout << "TrainingManager: agent prefab needs a component derived from Agent" << std::endl;
		entity->destroy();
		return false;
	}

	// Навчається лише нейромережевий мозок; інший мозок з образу йому заважав би
	NeuroevolutionBrain* brain = entity->getComponent<NeuroevolutionBrain>();

	for (AgentBrain* other : entity->getComponents<AgentBrain>())
	{
		if (other != brain) entity->removeComponent(other);
	}

	if (!brain) brain = entity->addComponent<NeuroevolutionBrain>();

	agent->setTraining(true);
	agent->maxEpisodeSteps = maxEpisodeSteps;

	if (ControllableBody* controllable = entity->getComponent<ControllableBody>()) controllable->setTrainingMode(true);

	if (RigidBody* body = entity->getComponent<RigidBody>())
	{
		body->setCollisionGroup((unsigned int)std::max(0, collisionGroup));
		body->setReportCollisions(true);
	}

	member.entity = entity;
	member.agent = agent;
	member.brain = brain;

	return true;
}

// Повертає агента в сцену й починає йому новий епізод
void TrainingManager::beginEpisode(Member& member, const EpisodeSetup& setup)
{
	// Актор має повернутися в сцену до того, як агент задаватиме йому положення й швидкість
	member.entity->isActiveSelf = true;

	if (RigidBody* body = member.entity->getComponent<RigidBody>()) body->syncActivity();

	member.agent->beginEpisode(setup);
}

// Починає всім агентам популяції поточний епізод
void TrainingManager::startEpisode()
{
	const EpisodePlan& plan = mPlans[mEpisode];

	EpisodeSetup setup;
	setup.environment = environment;
	setup.variant = plan.variant;
	setup.seed = plan.seed;

	for (size_t i = 0; i < mMembers.size(); i++)
	{
		// Параметри особини підставляються на першому епізоді покоління, а після навчання всі показують найкращу
		if (mEpisode == 0)
		{
			mMembers[i].brain->setParameters(*mMembers[i].agent, mFinished ? mAlgorithm.getBest() : mAlgorithm.getIndividual((int)i));
		}

		beginEpisode(mMembers[i], setup);
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

	// Без перевірного середовища зберігається найкраща особина кожного покоління
	if (!mValidator.entity)
	{
		NeuralNetwork best(mLayers);
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

	int episodes = std::max(1, episodesPerGeneration);

	// Варіанти середовища перемішуються, тож кожне покоління бачить їх у різному порядку, а за достатньої кількості епізодів - усі
	std::vector<int> order(mVariants);

	for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;

	std::shuffle(order.begin(), order.end(), random);

	mPlans.resize(episodes);

	for (int e = 0; e < episodes; e++)
	{
		mPlans[e].variant = order[e % order.size()];
		mPlans[e].seed = (unsigned int)random();
	}
}

// Починає перевірному агентові епізод з потрібним варіантом середовища
void TrainingManager::beginValidationEpisode()
{
	EpisodeSetup setup;
	setup.environment = validationEnvironment;
	setup.variant = mValidationVariant;
	setup.validation = true;

	beginEpisode(mValidator, setup);
}

// Веде перевірку найкращої особини в перевірному середовищі й зберігає мережу, що показала себе найкраще
void TrainingManager::updateValidation()
{
	if (!mValidator.entity) return;

	// Перевірка йде: чекаємо, поки агент завершить поточний епізод
	if (mValidationGeneration >= 0)
	{
		if (!mValidator.agent->isEpisodeDone()) return;

		mValidationScore += mValidator.agent->getEpisodeScore();
		mValidationReward += mValidator.agent->getEpisodeReward();
		mValidationVariant++;

		// Наступний варіант перевірного середовища для тієї самої особини
		if (mValidationVariant < mValidationVariants)
		{
			beginValidationEpisode();
			return;
		}

		float average = mValidationScore / (float)mValidationVariants;
		float reward = mValidationReward / (float)mValidationVariants;

		mValidationHistory.push_back(average);

		// Зберігається мережа з найбільшою оцінкою перевірки, а за рівної оцінки - з більшою винагородою
		float tolerance = 1e-3f * std::max(1.0f, std::fabs(mBestValidation));
		bool better = !mHasBestValidation || average > mBestValidation + tolerance || (std::fabs(average - mBestValidation) <= tolerance && reward > mBestValidationReward);

		if (better)
		{
			mHasBestValidation = true;
			mBestValidation = average;
			mBestValidationReward = reward;
			mBestValidationGeneration = mValidationGeneration + 1;

			NeuralNetwork network(mLayers);
			network.setParameters(mValidationParameters);
			mSaved = network.save(weightsAsset, mBestValidationGeneration, reward);
		}

		std::cout << "Validation of generation " << mValidationGeneration + 1 << ": score " << average << (better ? ", saved" : "") << std::endl;

		mValidatedGeneration = mValidationGeneration;
		mValidationGeneration = -1;
		mValidator.entity->isActiveSelf = false;
	}

	// Нова перевірка починається, щойно з'явилася найкраща особина ще не перевіреного покоління
	int latest = mAlgorithm.getGeneration() - 1;

	if (latest < 0 || latest == mValidatedGeneration) return;

	mValidationParameters = mAlgorithm.getBest();
	mValidationGeneration = latest;
	mValidationVariant = 0;
	mValidationScore = 0.0f;
	mValidationReward = 0.0f;

	mValidator.brain->setParameters(*mValidator.agent, mValidationParameters);
	beginValidationEpisode();
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
		ImGui::TextWrapped("Training is not running: the agent prefab needs a component derived from Agent (see the console).");
		ImGui::End();
		return;
	}

	int alive = 0;

	for (const Member& member : mMembers)
	{
		if (!member.agent->isEpisodeDone()) alive++;
	}

	ImGui::Text("Generation %d / %d%s", mAlgorithm.getGeneration() + (mFinished ? 0 : 1), generations, mFinished ? " (finished)" : "");

	if (!mPlans.empty() && mEpisode < (int)mPlans.size())
	{
		ImGui::Text("Episode %d / %d, variant %d / %d, step %d", mEpisode + 1, (int)mPlans.size(), mPlans[mEpisode].variant + 1, mVariants, mEpisodeStep);
	}

	ImGui::Text("Agents running: %d / %d", alive, (int)mMembers.size());
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

	if (mValidator.entity)
	{
		ImGui::Separator();

		if (mHasBestValidation)
		{
			ImGui::Text("Validation best score: %.2f (generation %d)", mBestValidation, mBestValidationGeneration);
		}
		else
		{
			ImGui::TextDisabled("Validation starts after the first generation");
		}

		if (!mValidationHistory.empty())
		{
			float low = *std::min_element(mValidationHistory.begin(), mValidationHistory.end());
			float high = *std::max_element(mValidationHistory.begin(), mValidationHistory.end());

			ImGui::PlotLines("Validation", mValidationHistory.data(), (int)mValidationHistory.size(), 0, nullptr, std::min(low, 0.0f), high, ImVec2(0, 60));
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
	visitor.reference("environment", environment);
	visitor.reference("validationEnvironment", validationEnvironment);
	visitor.property("populationSize", populationSize);
	visitor.property("eliteCount", eliteCount);
	visitor.property("tournamentSize", tournamentSize);
	visitor.property("mutationSigma", mutationSigma, 0.001f);
	visitor.property("finalSigmaRatio", finalSigmaRatio, 0.01f);
	visitor.property("generations", generations);
	visitor.property("episodesPerGeneration", episodesPerGeneration);
	visitor.property("maxEpisodeSteps", maxEpisodeSteps);
	visitor.property("collisionGroup", collisionGroup);
	visitor.property("seed", seed);
	visitor.property("continueTraining", continueTraining);
	visitor.property("stepsPerFrame", stepsPerFrame);
	visitor.property("renderScene", renderScene);
	visitor.asset("weightsAsset", weightsAsset, NeuralNetwork::FILE_EXTENSION);

	// Популяцію вже створено, тож її розмір і решта меж застосовуються лише до нового навчання
	populationSize = std::max(1, populationSize);
	eliteCount = std::max(0, eliteCount);
	tournamentSize = std::max(1, tournamentSize);
	generations = std::max(1, generations);
	episodesPerGeneration = std::max(1, episodesPerGeneration);
	maxEpisodeSteps = std::max(1, maxEpisodeSteps);
	stepsPerFrame = std::max(1, std::min(stepsPerFrame, 200));
}
