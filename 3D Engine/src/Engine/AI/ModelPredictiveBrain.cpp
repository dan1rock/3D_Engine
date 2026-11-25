#include "ModelPredictiveBrain.h"
#include "Agent.h"
#include "ControllableBody.h"
#include "DynamicsModel.h"
#include "LearnedDynamics.h"
#include "Entity.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

// Розбирає рівні однієї дії з рядка чисел через кому чи пробіл; повторення й значення поза [-1; 1] відкидаються
static void parseNumbers(const std::string& text, std::vector<float>& levels)
{
	levels.clear();

	std::string number;

	for (size_t i = 0; i <= text.size(); i++)
	{
		char c = i < text.size() ? text[i] : ' ';
		bool part = (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+';

		if (part)
		{
			number += c;
			continue;
		}

		if (number.empty()) continue;

		char* end = nullptr;
		float value = std::strtof(number.c_str(), &end);

		if (end && *end == 0)
		{
			value = std::max(-1.0f, std::min(value, 1.0f));

			if (std::find(levels.begin(), levels.end(), value) == levels.end()) levels.push_back(value);
		}

		number.clear();
	}
}

// Розбирає набори рівнів дій, якщо їх змінили
void ModelPredictiveBrain::parseLevels(int actionSize)
{
	if (actionLevels == mParsedLevels && (int)mLevels.size() == actionSize) return;

	mParsedLevels = actionLevels;
	mLevels.assign(actionSize, std::vector<float>());

	// Набори різних дій розділено крапкою з комою; набір, якого бракує, - три рівні
	size_t begin = 0;

	for (int d = 0; d < actionSize; d++)
	{
		size_t end = begin <= actionLevels.size() ? actionLevels.find(';', begin) : std::string::npos;
		std::string part = begin <= actionLevels.size() ? actionLevels.substr(begin, end == std::string::npos ? std::string::npos : end - begin) : std::string();

		parseNumbers(part, mLevels[d]);

		if (mLevels[d].empty()) mLevels[d] = { -1.0f, 0.0f, 1.0f };

		begin = end == std::string::npos ? actionLevels.size() + 1 : end + 1;
	}
}

// Повертає модель динаміки з ресурсу або nullptr
const LearnedDynamics* ModelPredictiveBrain::getModel() const
{
	return DynamicsModel::loadShared(modelAsset);
}

// Відкидає складений план, щоб наступне рішення склало новий
void ModelPredictiveBrain::resetPlan()
{
	mPlan.clear();
	mPlanStep = 0;
}

// Відкидає старий план на початку епізоду
void ModelPredictiveBrain::onEpisodeBegin(Agent& agent)
{
	resetPlan();
}

// Повертає довжину прогнозу в кроках для поточного стану
int ModelPredictiveBrain::getPlanningSteps(const float* state) const
{
	return std::max(1, horizon);
}

// Повертає, скільки кроків кандидат тримає дію з набору
int ModelPredictiveBrain::getHoldSteps(const float* state, int steps) const
{
	return std::max(1, std::min(steps, (int)std::round(holdTime / SimulationSettings::fixedStep)));
}

// Виконує дію зі складеного плану, а коли настав час - складає новий
void ModelPredictiveBrain::decide(Agent& agent, const float* observation, float* action)
{
	ControllableBody* body = agent.getControllableBody();
	const LearnedDynamics* model = getModel();
	int actions = agent.getActionSize();

	// Без моделі, що підходить до керованого об'єкта, планувати нічим: агент стоїть
	if (!model || !body || !body->getBody() || !DynamicsModel::isCompatible(*model, *body) || model->getActionSize() != actions)
	{
		std::fill(action, action + actions, 0.0f);
		mPlannedPath.clear();
		mCandidatePaths.clear();
		return;
	}

	int interval = std::max(1, replanInterval);

	if (mPlan.empty() || mActionSize != actions || mPlanStep >= interval || (mPlanStep + 1) * actions > (int)mPlan.size())
	{
		plan(agent, *model, *body);
	}

	std::copy(mPlan.begin() + (size_t)mPlanStep * actions, mPlan.begin() + (size_t)(mPlanStep + 1) * actions, action);
	mPlanStep++;
}

// Складає новий план: перебирає поєднання дій із наборів і запам'ятовує найкраще
void ModelPredictiveBrain::plan(Agent& agent, const LearnedDynamics& model, ControllableBody& body)
{
	int stateSize = model.getStateSize();
	int actionSize = model.getActionSize();
	float dt = SimulationSettings::fixedStep;

	mActionSize = actionSize;
	parseLevels(actionSize);

	// Кандидати - усі поєднання рівнів; остання дія змінюється найшвидше
	int count = 1;

	for (const std::vector<float>& levels : mLevels) count *= (int)levels.size();

	std::vector<float> state(stateSize);
	std::vector<float> current(actionSize);
	float x0, z0, heading0;

	body.getState(state.data());
	body.getActions(current.data());
	body.getPlanarPose(x0, z0, heading0);

	int steps = std::max(1, getPlanningSteps(state.data()));
	int holdSteps = std::max(1, std::min(steps, getHoldSteps(state.data(), steps)));

	beginPlanning(agent, state.data(), x0, z0, heading0, count, steps);

	// Дію з набору кандидат тримає holdSteps кроків, а далі дії додаються під час прогнозу
	size_t sequenceLength = (size_t)steps * actionSize;
	mSequences.assign((size_t)count * sequenceLength, 0.0f);

	for (int k = 0; k < count; k++)
	{
		float* sequence = mSequences.data() + (size_t)k * sequenceLength;
		int index = k;

		for (int d = actionSize - 1; d >= 0; d--)
		{
			int levels = (int)mLevels[d].size();
			float value = mLevels[d][index % levels];
			index /= levels;

			for (int h = 0; h < holdSteps; h++) sequence[h * actionSize + d] = value;
		}
	}

	// Усі кандидати прогнозуються разом: мережа обчислює їхні кроки одним пакетом
	mStates.resize((size_t)count * stateSize);
	mNextStates.resize(mStates.size());
	mActions.resize((size_t)count * actionSize);
	mPositions.resize((size_t)count * steps * 2);

	std::vector<float> xs(count, x0), zs(count, z0), headings(count, heading0), scores(count, 0.0f);

	for (int k = 0; k < count; k++) std::copy(state.begin(), state.end(), mStates.data() + (size_t)k * stateSize);

	for (int h = 0; h < steps; h++)
	{
		for (int k = 0; k < count; k++)
		{
			float* applied = mSequences.data() + (size_t)k * sequenceLength + (size_t)h * actionSize;

			// Після утримання дії з набору дію підбирає похідний клас: так видно, куди веде саме початковий вибір
			if (h >= holdSteps)
			{
				std::copy(applied - actionSize, applied, applied);
				continueAction(k, h, mStates.data() + (size_t)k * stateSize, xs[k], zs[k], headings[k], applied);

				for (int d = 0; d < actionSize; d++) applied[d] = std::max(-1.0f, std::min(applied[d], 1.0f));
			}

			std::copy(applied, applied + actionSize, mActions.data() + (size_t)k * actionSize);
		}

		model.predictBatch(mStates.data(), mActions.data(), count, mNextStates.data());

		for (int k = 0; k < count; k++)
		{
			const float* next = mNextStates.data() + (size_t)k * stateSize;
			const float* applied = mActions.data() + (size_t)k * actionSize;
			const float* previous = h == 0 ? current.data() : mSequences.data() + (size_t)k * sequenceLength + (size_t)(h - 1) * actionSize;

			LearnedDynamics::integratePose(xs[k], zs[k], headings[k], next, dt);

			mPositions[((size_t)k * steps + h) * 2] = xs[k];
			mPositions[((size_t)k * steps + h) * 2 + 1] = zs[k];

			scores[k] += evaluateStep(k, h, next, applied, previous, xs[k], zs[k], headings[k]);
		}

		mStates.swap(mNextStates);
	}

	for (int k = 0; k < count; k++)
	{
		scores[k] += evaluateEnd(k, mStates.data() + (size_t)k * stateSize, xs[k], zs[k], headings[k]);
	}

	// Навчена модель зіткнень оцінює кожну дію з набору за тим, що зараз навколо об'єкта
	if (model.hasCollisionModel() && collisionWeight > 0.0f)
	{
		std::vector<float> scan(model.getCollisionRays());
		DynamicsModel::scanSurroundings(&body, model, scan.data());

		for (int k = 0; k < count; k++)
		{
			const float* first = mSequences.data() + (size_t)k * sequenceLength;
			scores[k] -= collisionWeight * model.predictCollision(scan.data(), state.data(), first);
		}
	}

	int best = 0;

	for (int k = 1; k < count; k++)
	{
		if (scores[k] > scores[best]) best = k;
	}

	mPlan.assign(mSequences.begin() + (size_t)best * sequenceLength, mSequences.begin() + (size_t)(best + 1) * sequenceLength);
	mPlanStep = 0;

	// Шлях для малювання - на висоті об'єкта
	float height = getOwner()->getTransform()->getPosition().y;

	// Складає прогнозований шлях кандидата k від поточного положення
	auto buildPath = [&](int k, std::vector<Vector3>& path)
	{
		path.clear();
		path.reserve(steps + 1);
		path.push_back(Vector3(x0, height, z0));

		for (int h = 0; h < steps; h++)
		{
			path.push_back(Vector3(mPositions[((size_t)k * steps + h) * 2], height, mPositions[((size_t)k * steps + h) * 2 + 1]));
		}
	};

	buildPath(best, mPlannedPath);

	// Решта кандидатів запам'ятовується впорядкованою за оцінкою: кращі малюватимуться пізніше, тобто поверх гірших
	mCandidatePaths.clear();

	if (!showCandidates) return;

	std::vector<int> order;

	for (int k = 0; k < count; k++)
	{
		if (k != best) order.push_back(k);
	}

	std::sort(order.begin(), order.end(), [&scores](int a, int b) { return scores[a] < scores[b]; });

	mCandidatePaths.resize(order.size());

	for (size_t i = 0; i < order.size(); i++) buildPath(order[i], mCandidatePaths[i]);
}

// Малює вибраний і розглянуті прогнозовані шляхи
void ModelPredictiveBrain::update()
{
	// Розглянуті шляхи малюються першими, щоб вибраний лишався поверх; колір - за місцем в оцінці, бо самі оцінки через штрафи різняться на порядки
	if (showCandidates && !mCandidatePaths.empty())
	{
		size_t total = mCandidatePaths.size();

		for (size_t i = 0; i < total; i++)
		{
			float t = total > 1 ? (float)i / (float)(total - 1) : 1.0f;

			// Від темно-синього напівпрозорого для найгірших до блакитного щільнішого для найкращих
			int red = (int)(30 + 60 * t);
			int green = (int)(60 + 160 * t);
			int blue = (int)(160 + 95 * t);
			int alpha = (int)(50 + 130 * t);

			DynamicsModel::drawPath(mCandidatePaths[i], IM_COL32(red, green, blue, alpha), 1.0f);
		}
	}

	if (showPlan && !mPlannedPath.empty()) DynamicsModel::drawPath(mPlannedPath, IM_COL32(255, 160, 40, 255), 3.0f);
}

// Перелічує власні поля для файлу сцени та інспектора
void ModelPredictiveBrain::visitProperties(PropertyVisitor& visitor)
{
	visitor.asset("modelAsset", modelAsset, LearnedDynamics::FILE_EXTENSION);
	visitor.property("actionLevels", actionLevels);
	visitor.property("holdTime", holdTime, 0.01f);
	visitor.property("horizon", horizon);
	visitor.property("replanInterval", replanInterval);
	visitor.property("collisionWeight", collisionWeight, 0.5f);
	visitor.property("showPlan", showPlan);
	visitor.property("showCandidates", showCandidates);

	horizon = std::max(1, std::min(horizon, 500));
	replanInterval = std::max(1, replanInterval);
}
