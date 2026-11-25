#include "CarAgent.h"
#include "VehicleController.h"
#include "RaySensor.h"
#include "TrajectoryPredictor.h"
#include "Entity.h"
#include "RigidBody.h"
#include "EntityManager.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include <algorithm>
#include <cmath>
#include <random>

// Обмежує значення спостереження проміжком [-1; 1]
static float clampUnit(float value)
{
	return std::max(-1.0f, std::min(value, 1.0f));
}

// Повертає розмір вектора спостережень
int CarAgent::getObservationSize() const
{
	return OBSERVATION_SIZE;
}

// Повертає кількість маршрутів у середовищі
int CarAgent::getVariantCount(Entity* environment) const
{
	std::vector<Entity*> routes;
	RouteTracker::collectRoutes(environment, routes);

	return std::max(1, (int)routes.size());
}

// Повертає пройдену частку маршруту у відсотках
float CarAgent::getEpisodeScore() const
{
	return mRoute.getCompletion() * 100.0f;
}

// Повертає стан руху маршрутом з останнього кроку
const RouteTracker& CarAgent::getRouteTracker() const
{
	return mRoute;
}

// Повертає кількість найближчих рухомих об'єктів з останнього кроку і записує їхні положення, швидкості та тіла
int CarAgent::getTrackedObjects(Vector3* positions, Vector3* velocities, RigidBody** bodies) const
{
	for (int i = 0; i < mTrackedCount; i++)
	{
		positions[i] = mTrackedPositions[i];
		velocities[i] = mTrackedVelocities[i];
		if (bodies) bodies[i] = mTrackedBodies[i];
	}

	return mTrackedCount;
}

// Перевіряє, чи автомобіль уже зібрано, і вмикає облік його зіткнень
bool CarAgent::isReady()
{
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();

	if (!vehicle || !vehicle->getBody()) return false;

	// Зіткнення потрібні для винагороди, тож їх облік вмикає сам агент
	vehicle->getBody()->setReportCollisions(true);

	return true;
}

// Ставить автомобіль на маршрут із середовища: випадкова точка, зсув і курс або початок маршруту для перевірки
void CarAgent::onEpisodeBegin(const EpisodeSetup& setup)
{
	mHasPrevious = false;
	mStuckSteps = 0;
	mTrackedCount = 0;

	std::vector<Entity*> routes;
	RouteTracker::collectRoutes(setup.environment ? setup.environment : route, routes);

	if (routes.empty())
	{
		mRoute.setRoute(route, 0);
		return;
	}

	route = routes[std::max(0, setup.variant) % routes.size()];

	std::vector<Vector3> waypoints;
	RouteTracker::collectWaypoints(route, waypoints);

	if (waypoints.size() < 2)
	{
		mRoute.setRoute(route, 0);
		return;
	}

	int start = 0;
	float offset = 0.0f;
	float angle = 0.0f;

	// Перевірка завжди йде з початку маршруту, а навчання - з випадкових умов, однакових для всієї популяції
	if (!setup.validation)
	{
		std::mt19937 random(setup.seed);
		std::uniform_real_distribution<float> unit(-1.0f, 1.0f);

		// Старт вибирається серед точок, після яких лишається достатня частина маршруту
		float total = RouteTracker::pathLength(waypoints, 0);
		std::vector<int> candidates;

		for (int i = 0; i + 1 < (int)waypoints.size(); i++)
		{
			if (RouteTracker::pathLength(waypoints, i) >= minStartRemaining * total) candidates.push_back(i);
		}

		if (candidates.empty()) candidates.push_back(0);

		start = candidates[std::uniform_int_distribution<int>(0, (int)candidates.size() - 1)(random)];
		offset = unit(random) * startOffset;
		angle = unit(random) * startAngle * 3.14159265f / 180.0f;
	}

	Vector3 direction = waypoints[start + 1] - waypoints[start];
	direction.y = 0.0f;
	direction.normalize();

	// Зсув убік рахується від напряму маршруту: праворуч від нього при додатному значенні
	Vector3 right(direction.z, 0.0f, -direction.x);
	Vector3 position = waypoints[start] + right * offset + Vector3(0.0f, spawnHeight, 0.0f);

	teleport(position, Vector3(0.0f, std::atan2(direction.x, direction.z) + angle, 0.0f));
	mRoute.setRoute(route, start);
}

// Записує промені, власний рух, напрям на маршрут і найближчі рухомі об'єкти
void CarAgent::collectObservations(float* out)
{
	std::fill(out, out + OBSERVATION_SIZE, 0.0f);

	Transform* transform = getOwner()->getTransform();
	Vector3 position = transform->getPosition();
	Vector3 forward = transform->getForward();
	Vector3 right = transform->getRight();
	Vector3 up = transform->getUp();

	// Відстані до перешкод; без сенсора агент вважає, що перешкод немає
	RaySensor* rays = getOwner()->getComponent<RaySensor>();

	if (rays && rays->rayCount == RAY_COUNT)
	{
		rays->sense(out);
	}
	else
	{
		std::fill(out, out + RAY_COUNT, 1.0f);
	}

	// Власний рух у системі координат корпусу
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();
	RigidBody* body = vehicle->getBody();

	float maxSpeed = vehicle->maxSpeed > 0.0f ? vehicle->maxSpeed : 70.0f;
	Vector3 velocity = body->getVelocity();
	Vector3 angular = body->getAngularVelocity();

	out[9] = clampUnit(velocity * forward / maxSpeed);
	out[10] = clampUnit(velocity * right / maxSpeed);
	out[11] = clampUnit(angular * up / maxYawRate);
	out[12] = vehicle->maxSteering > 0.0f ? clampUnit(vehicle->getSteering() / vehicle->maxSteering) : 0.0f;

	// Маршрут могли змінити в інспекторі: тоді він починається заново
	if (mRoute.getRoute() != route) mRoute.setRoute(route, 0);

	mRoute.lookahead = lookahead;
	mRoute.goalRadius = goalRadius;
	mRoute.update(position);

	// Напрям і відстань до точки маршруту попереду агента
	if (!mRoute.getWaypoints().empty())
	{
		Vector3 toTarget = mRoute.getTarget() - position;

		float angle = std::atan2(toTarget * right, toTarget * forward);
		float dx = toTarget.x;
		float dz = toTarget.z;

		out[13] = std::sin(angle);
		out[14] = std::cos(angle);
		out[15] = clampUnit(std::sqrt(dx * dx + dz * dz) / range);
	}
	else
	{
		out[14] = 1.0f;
	}

	// Найближчі рухомі об'єкти
	observeObjects(position, velocity, out + 16);
}

// Нараховує винагороду за просування й завершує епізод на цілі, а під час навчання - і на невдачі
void CarAgent::onStep()
{
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();
	RigidBody* body = vehicle->getBody();

	float remaining = mRoute.getRemainingDistance();
	bool collided = body->takeCollisions() > 0;

	// Винагорода переходу з попереднього стану в поточний: просування, зіткнення, ривок кермом і плата за час
	if (mHasPrevious)
	{
		float maxSpeed = vehicle->maxSpeed > 0.0f ? vehicle->maxSpeed : 70.0f;

		float reward = PROGRESS_WEIGHT * (mPreviousRemaining - remaining) / (maxSpeed * SimulationSettings::fixedStep);
		reward -= COLLISION_WEIGHT * (collided ? 1.0f : 0.0f);
		reward -= STEERING_WEIGHT * std::fabs(vehicle->getSteeringAction() - mPreviousSteering);
		reward -= TIME_WEIGHT;

		addReward(reward);

		Transform* transform = getOwner()->getTransform();
		float speed = std::fabs(body->getVelocity() * transform->getForward());

		mStuckSteps = speed < stuckSpeed ? mStuckSteps + 1 : 0;

		// Ціль завершує епізод завжди: агент зупиняється в кінці маршруту
		if (mRoute.isComplete()) endEpisode(EpisodeEnd::Success, "goal");

		// Невдачі обривають епізод лише під час навчання; поза ним автомобіль їде далі
		else if (isTraining())
		{
			// Корпус, нахилений більш ніж на ~70 градусів, вважається перевернутим
			if (collided) endEpisode(EpisodeEnd::Failure, "collision");
			else if (transform->getUp().y < 0.3f) endEpisode(EpisodeEnd::Failure, "flipped");
			else if (mStuckSteps * SimulationSettings::fixedStep >= stuckTime) endEpisode(EpisodeEnd::Failure, "stuck");
		}
	}

	mPreviousRemaining = remaining;
	mPreviousSteering = vehicle->getSteeringAction();
	mHasPrevious = true;
}

// Записує положення двох найближчих рухомих об'єктів зараз і через predictionTime
void CarAgent::observeObjects(const Vector3& position, const Vector3& velocity, float* out)
{
	Transform* transform = getOwner()->getTransform();
	Vector3 forward = transform->getForward();
	Vector3 right = transform->getRight();

	RigidBody* own = getOwner()->getComponent<RigidBody>();
	unsigned int group = own ? own->getCollisionGroup() : 0;

	// Найближчі об'єкти в межах дальності: відстань і саме тіло
	std::pair<float, RigidBody*> nearest[TRACKED_OBJECTS];

	for (int i = 0; i < TRACKED_OBJECTS; i++) nearest[i] = std::make_pair(range, (RigidBody*)nullptr);

	for (const auto& entry : EntityManager::get()->getRigidBodies())
	{
		RigidBody* body = entry.second;

		if (body == own || body->isStatic()) continue;
		if (!body->getOwner()->isActive()) continue;

		// Агентів своєї групи зіткнень агент не бачить, бо й не може з ними зіткнутися
		if (group != 0 && body->getCollisionGroup() == group) continue;

		float distance = (body->getOwner()->getTransform()->getPosition() - position).length();

		for (int i = 0; i < TRACKED_OBJECTS; i++)
		{
			if (distance >= nearest[i].first) continue;

			for (int j = TRACKED_OBJECTS - 1; j > i; j--) nearest[j] = nearest[j - 1];

			nearest[i] = std::make_pair(distance, body);
			break;
		}
	}

	mTrackedCount = 0;

	TrajectoryPredictor* predictor = TrajectoryPredictor::getActive();

	// Відсутній об'єкт лишає нулі як у поточному, так і в прогнозованому положенні
	for (int i = 0; i < TRACKED_OBJECTS; i++)
	{
		RigidBody* body = nearest[i].second;

		if (!body) continue;

		mTrackedPositions[mTrackedCount] = body->getOwner()->getTransform()->getPosition();
		mTrackedVelocities[mTrackedCount] = body->getVelocity();
		mTrackedBodies[mTrackedCount] = body;
		mTrackedCount++;

		Vector3 relative = body->getOwner()->getTransform()->getPosition() - position;
		Vector3 predicted = relative + (body->getVelocity() - velocity) * predictionTime;

		// Прогнозувальник траєкторій сцени, якщо він є, передбачає рух об'єкта точніше за сталу швидкість
		Vector3 forecast;

		if (predictor && predictor->predict(body, predictionTime, forecast)) predicted = forecast - (position + velocity * predictionTime);

		out[i * 2] = clampUnit(relative * right / range);
		out[i * 2 + 1] = clampUnit(relative * forward / range);
		out[TRACKED_OBJECTS * 2 + i * 2] = clampUnit(predicted * right / range);
		out[TRACKED_OBJECTS * 2 + i * 2 + 1] = clampUnit(predicted * forward / range);
	}
}

// Перелічує власні поля для файлу сцени та інспектора
void CarAgent::visitProperties(PropertyVisitor& visitor)
{
	Agent::visitProperties(visitor);

	visitor.reference("route", route);
	visitor.property("range", range, 0.5f);
	visitor.property("maxYawRate", maxYawRate, 0.05f);
	visitor.property("predictionTime", predictionTime, 0.05f);
	visitor.property("goalRadius", goalRadius, 0.1f);
	visitor.property("lookahead", lookahead, 0.1f);
	visitor.property("stuckSpeed", stuckSpeed, 0.05f);
	visitor.property("stuckTime", stuckTime, 0.1f);
	visitor.property("spawnHeight", spawnHeight, 0.05f);
	visitor.property("startOffset", startOffset, 0.05f);
	visitor.property("startAngle", startAngle, 0.5f);
	visitor.property("minStartRemaining", minStartRemaining, 0.01f);

	if (range < 0.01f) range = 0.01f;
	if (maxYawRate < 0.01f) maxYawRate = 0.01f;
	minStartRemaining = std::max(0.0f, std::min(minStartRemaining, 1.0f));
}
