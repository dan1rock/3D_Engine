#include "AgentBrain.h"
#include "VehicleController.h"
#include "RigidBody.h"
#include "Entity.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include <cmath>

// Починає новий епізод: обнуляє винагороду, лічильники й ознаку завершення
void AgentBrain::beginEpisode()
{
	mEpisodeReward = 0.0f;
	mLastReward = 0.0f;
	mEpisodeSteps = 0;
	mStuckSteps = 0;
	mPreviousRemaining = 0.0f;
	mPreviousSteering = 0.0f;
	mHasPrevious = false;
	mEnd = EpisodeEnd::None;

	// Удари, що сталися ще до початку епізоду, йому не зараховуються
	if (RigidBody* body = getOwner()->getComponent<RigidBody>()) body->takeCollisions();
}

// Перевіряє, чи епізод завершено
bool AgentBrain::isEpisodeDone() const
{
	return mEnd != EpisodeEnd::None;
}

// Повертає причину завершення епізоду
EpisodeEnd AgentBrain::getEpisodeEnd() const
{
	return mEnd;
}

// Повертає суму винагород поточного епізоду
float AgentBrain::getEpisodeReward() const
{
	return mEpisodeReward;
}

// Повертає кількість кроків поточного епізоду
int AgentBrain::getEpisodeSteps() const
{
	return mEpisodeSteps;
}

// Повертає винагороду останнього кроку
float AgentBrain::getLastReward() const
{
	return mLastReward;
}

// Повертає останній вектор спостережень
const float* AgentBrain::getObservation() const
{
	return mObservation;
}

// Повертає назву причини завершення епізоду
const char* AgentBrain::episodeEndName(EpisodeEnd end)
{
	switch (end)
	{
	case EpisodeEnd::Goal: return "goal";
	case EpisodeEnd::Collision: return "collision";
	case EpisodeEnd::Flipped: return "flipped";
	case EpisodeEnd::Stuck: return "stuck";
	case EpisodeEnd::Timeout: return "timeout";
	default: return "running";
	}
}

// Читає сенсори, рахує винагороду переходу та передає автомобілю нову дію
void AgentBrain::fixedUpdate()
{
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();
	AgentSensor* sensor = getOwner()->getComponent<AgentSensor>();

	if (!vehicle || !sensor) return;

	RigidBody* body = vehicle->getRigidBody();

	// Автомобіль ще не зібрано: керувати поки нічим
	if (!body) return;

	// Зіткнення потрібні для винагороди, тому їх облік вмикає сам мозок
	body->setReportCollisions(true);

	if (isEpisodeDone())
	{
		vehicle->setActions(0.0f, 0.0f);
		return;
	}

	sensor->observe(mObservation);

	float remaining = sensor->getRemainingDistance();
	bool collided = body->takeCollisions() > 0;

	// Винагорода переходу з попереднього стану в поточний: просування, зіткнення, ривок кермом і плата за час
	if (mHasPrevious)
	{
		float maxSpeed = vehicle->maxSpeed > 0.0f ? vehicle->maxSpeed : 70.0f;

		float reward = PROGRESS_WEIGHT * (mPreviousRemaining - remaining) / (maxSpeed * SimulationSettings::fixedStep);
		reward -= COLLISION_WEIGHT * (collided ? 1.0f : 0.0f);
		reward -= STEERING_WEIGHT * std::fabs(vehicle->getSteeringAction() - mPreviousSteering);
		reward -= TIME_WEIGHT;

		mLastReward = reward;
		mEpisodeReward += reward;
		mEpisodeSteps++;

		checkEpisodeEnd(vehicle, sensor, collided);
	}

	mPreviousRemaining = remaining;
	mPreviousSteering = vehicle->getSteeringAction();
	mHasPrevious = true;

	if (isEpisodeDone())
	{
		vehicle->setActions(0.0f, 0.0f);
		return;
	}

	float action[ACTION_SIZE] = {};
	decide(mObservation, action);

	vehicle->setActions(action[0], action[1]);
}

// Перевіряє умови завершення епізоду після кроку
void AgentBrain::checkEpisodeEnd(VehicleController* vehicle, AgentSensor* sensor, bool collided)
{
	// Ціль завершує епізод завжди: навчений агент зупиняється в кінці маршруту
	if (sensor->isRouteComplete())
	{
		mEnd = EpisodeEnd::Goal;
		return;
	}

	if (!episodic) return;

	if (collided)
	{
		mEnd = EpisodeEnd::Collision;
		return;
	}

	Transform* transform = getOwner()->getTransform();

	// Корпус, нахилений більш ніж на ~70 градусів, вважається перевернутим
	if (transform->getUp().y < 0.3f)
	{
		mEnd = EpisodeEnd::Flipped;
		return;
	}

	float speed = std::fabs(vehicle->getRigidBody()->getVelocity() * transform->getForward());

	mStuckSteps = speed < stuckSpeed ? mStuckSteps + 1 : 0;

	if (mStuckSteps * SimulationSettings::fixedStep >= stuckTime)
	{
		mEnd = EpisodeEnd::Stuck;
		return;
	}

	if (mEpisodeSteps >= maxEpisodeSteps)
	{
		mEnd = EpisodeEnd::Timeout;
	}
}

// Перелічує власні поля для файлу сцени та інспектора
void AgentBrain::visitProperties(PropertyVisitor& visitor)
{
	visitor.property("maxEpisodeSteps", maxEpisodeSteps);
	visitor.property("stuckSpeed", stuckSpeed, 0.05f);
	visitor.property("stuckTime", stuckTime, 0.1f);
}
