#include "Agent.h"
#include "AgentBrain.h"
#include "ControllableBody.h"
#include "RigidBody.h"
#include "Entity.h"
#include "Properties.h"
#include <algorithm>

// Повертає розмір вектора дій; за замовчуванням - скільки дій приймає керований об'єкт агента
int Agent::getActionSize() const
{
	ControllableBody* body = mOwner ? mOwner->getComponent<ControllableBody>() : nullptr;

	return body ? body->getActionSize() : 0;
}

// Повертає оцінку завершеного епізоду для перевірки; за замовчуванням - сума винагород
float Agent::getEpisodeScore() const
{
	return mEpisodeReward;
}

// Починає новий епізод: обнуляє винагороду й лічильники та дає агентові розставити себе
void Agent::beginEpisode(const EpisodeSetup& setup)
{
	mEpisodeReward = 0.0f;
	mStepReward = 0.0f;
	mLastReward = 0.0f;
	mEpisodeSteps = 0;
	mEnd = EpisodeEnd::None;
	mEndReason.clear();

	onEpisodeBegin(setup);

	if (AgentBrain* brain = getBrain()) brain->onEpisodeBegin(*this);

	// Удари, що сталися ще до початку епізоду, йому не зараховуються
	if (RigidBody* body = getOwner()->getComponent<RigidBody>()) body->takeCollisions();
}

// Завершує епізод з указаним результатом і причиною для журналу
void Agent::endEpisode(EpisodeEnd end, const char* reason)
{
	if (mEnd != EpisodeEnd::None || end == EpisodeEnd::None) return;

	mEnd = end;
	mEndReason = reason ? reason : episodeEndName(end);
}

// Додає винагороду поточного кроку
void Agent::addReward(float reward)
{
	mStepReward += reward;
	mEpisodeReward += reward;
}

// Перевіряє, чи епізод завершено
bool Agent::isEpisodeDone() const
{
	return mEnd != EpisodeEnd::None;
}

// Повертає результат завершеного епізоду
EpisodeEnd Agent::getEpisodeEnd() const
{
	return mEnd;
}

// Повертає причину завершення епізоду
const std::string& Agent::getEndReason() const
{
	return mEndReason;
}

// Повертає суму винагород поточного епізоду
float Agent::getEpisodeReward() const
{
	return mEpisodeReward;
}

// Повертає кількість кроків поточного епізоду
int Agent::getEpisodeSteps() const
{
	return mEpisodeSteps;
}

// Повертає винагороду останнього кроку
float Agent::getLastReward() const
{
	return mLastReward;
}

// Повертає останній вектор спостережень
const float* Agent::getObservation() const
{
	return mObservation.data();
}

// Повертає останній вектор дій
const float* Agent::getLastAction() const
{
	return mAction.data();
}

// Вмикає режим навчання: тренер завершує епізоди невдачами й лімітом кроків
void Agent::setTraining(bool training)
{
	mTraining = training;
}

// Перевіряє, чи агент навчається
bool Agent::isTraining() const
{
	return mTraining;
}

// Вмикає чи вимикає рішення агента; вимкнений агент не керує тілом, наприклад коли дії задає тренер моделі динаміки
void Agent::setControlEnabled(bool enabled)
{
	mControlEnabled = enabled;
}

// Повертає мозок агента на тому самому об'єкті або nullptr
AgentBrain* Agent::getBrain()
{
	return getOwner()->getComponent<AgentBrain>();
}

// Повертає керований об'єкт агента на тому самому об'єкті або nullptr
ControllableBody* Agent::getControllableBody()
{
	return getOwner()->getComponent<ControllableBody>();
}

// Повертає назву результату епізоду
const char* Agent::episodeEndName(EpisodeEnd end)
{
	switch (end)
	{
	case EpisodeEnd::Success: return "success";
	case EpisodeEnd::Failure: return "failure";
	case EpisodeEnd::Timeout: return "timeout";
	default: return "running";
	}
}

// Виконує дію; за замовчуванням передає її керованому об'єкту
void Agent::applyActions(const float* action)
{
	if (ControllableBody* body = getControllableBody()) body->setActions(action);
}

// Зупиняє агента після завершення епізоду; за замовчуванням задає нульові дії
void Agent::onEpisodeEnd()
{
	ControllableBody* body = getControllableBody();

	if (!body) return;

	std::vector<float> zero(std::max(1, body->getActionSize()), 0.0f);
	body->setActions(zero.data());
}

// Переносить агента в нове положення з нульовими швидкостями й скинутим керуванням
void Agent::teleport(const Vector3& position, const Vector3& rotation)
{
	Transform* transform = getOwner()->getTransform();
	transform->setPosition(position);
	transform->setRotation(rotation);

	if (RigidBody* body = getOwner()->getComponent<RigidBody>())
	{
		body->setVelocity(Vector3());
		body->setAngularVelocity(Vector3());
		body->takeCollisions();
	}

	if (ControllableBody* controllable = getControllableBody()) controllable->resetControl();
}

// Збирає спостереження, рахує винагороду кроку та виконує рішення мозку
void Agent::fixedUpdate()
{
	if (!mControlEnabled || !isReady()) return;

	if (isEpisodeDone())
	{
		onEpisodeEnd();
		return;
	}

	mObservation.resize(std::max(1, getObservationSize()));
	mAction.assign(std::max(1, getActionSize()), 0.0f);

	collectObservations(mObservation.data());

	// Винагорода кроку - наслідок попередньої дії, який видно в щойно зібраних спостереженнях
	mStepReward = 0.0f;
	onStep();
	mLastReward = mStepReward;
	mEpisodeSteps++;

	if (!isEpisodeDone() && mTraining && mEpisodeSteps >= maxEpisodeSteps) endEpisode(EpisodeEnd::Timeout);

	if (isEpisodeDone())
	{
		onEpisodeEnd();
		return;
	}

	// Мозок вирішує, а без нього дію може дати сам агент, наприклад з клавіатури
	AgentBrain* brain = getBrain();
	bool decided = true;

	if (brain) brain->decide(*this, mObservation.data(), mAction.data());
	else decided = heuristic(mAction.data());

	if (decided) applyActions(mAction.data());
}

// Перелічує власні поля для файлу сцени та інспектора
void Agent::visitProperties(PropertyVisitor& visitor)
{
	visitor.property("maxEpisodeSteps", maxEpisodeSteps);

	if (maxEpisodeSteps < 1) maxEpisodeSteps = 1;
}
