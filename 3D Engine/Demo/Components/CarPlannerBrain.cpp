#include "CarPlannerBrain.h"
#include "VehicleController.h"
#include "DynamicsModel.h"
#include "LearnedDynamics.h"
#include "TrajectoryPredictor.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Entity.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include <algorithm>
#include <cmath>

// Найбільша довжина ламаної маршруту попереду, яку враховує оцінка; її має вистачати, щоб загальмувати перед поворотом, м
static const float ROUTE_WINDOW = 150.0f;
// Скільки точок має пряме продовження кінця прогнозованого шляху
static const int TERMINAL_POINTS = 10;

// Обмежує дію проміжком [-1; 1]
static float clampAction(float value)
{
	return std::max(-1.0f, std::min(value, 1.0f));
}

// Задає автомобільні набори дій: чотири рівні газу й дев'ять рівнів керма
CarPlannerBrain::CarPlannerBrain()
{
	modelAsset = "Assets\\Networks\\Vehicle Dynamics.dyn";
	actionLevels = "-1, 0, 0.5, 1; -1, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75, 1";
}

// Скидає виїзд і план на початку епізоду
void CarPlannerBrain::onEpisodeBegin(Agent& agent)
{
	ModelPredictiveBrain::onEpisodeBegin(agent);

	mStalledSteps = 0;
	mRecoverySteps = 0;
}

// Виїжджає з глухої пози, а інакше виконує план прогнозуючого керування
void CarPlannerBrain::decide(Agent& agent, const float* observation, float* action)
{
	CarAgent* car = dynamic_cast<CarAgent*>(&agent);
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();
	const LearnedDynamics* model = getModel();

	// Без автомобіля чи моделі рішення лишається базовому плануванню, яке в такому разі просто стоїть
	if (!car || !vehicle || !vehicle->getBody() || !model || !DynamicsModel::isCompatible(*model, *vehicle))
	{
		ModelPredictiveBrain::decide(agent, observation, action);
		return;
	}

	// Автомобіль, що застряг у позі, з якої жоден кандидат не веде вперед, виїжджає з неї, повертаючи ніс до маршруту
	std::vector<float> state(vehicle->getStateSize());
	vehicle->getState(state.data());

	mStalledSteps = std::fabs(state[0]) < 0.5f ? mStalledSteps + 1 : 0;

	if (mRecoverySteps == 0 && mStalledSteps * SimulationSettings::fixedStep >= recoveryDelay)
	{
		float x, z, heading;
		vehicle->getPlanarPose(x, z, heading);
		prepareRoute(*car, x, z, heading);

		int segment = 0;
		float deviation = 0.0f;
		float along = projectOnRoute(x, z, segment, deviation, (int)mRouteDistance.size());

		float tx, tz;
		pointOnRoute(along + 8.0f, tx, tz);

		float alpha = std::atan2(tx - x, tz - z) - heading;

		while (alpha > 3.14159265f) alpha -= 6.2831853f;
		while (alpha < -3.14159265f) alpha += 6.2831853f;

		// Огляд моделі зіткнень показує, де більше місця: позаду чи попереду; без нього автомобіль просто здає назад
		bool reverse = true;
		int rays = model->getCollisionRays();

		if (rays >= 4)
		{
			std::vector<float> scan(rays);
			DynamicsModel::scanSurroundings(vehicle, *model, scan.data());

			float front = std::min(scan[0], std::min(scan[1], scan[rays - 1]));
			float back = std::min(scan[rays / 2], std::min(scan[rays / 2 - 1], scan[rays / 2 + 1]));

			reverse = back > front;
		}

		// Задній хід з кермом убік, протилежний маршруту, розвертає ніс до маршруту; якщо позаду тісніше, автомобіль обережно повертає вперед
		mRecoveryThrottle = reverse ? -1.0f : 0.5f;
		mRecoverySteering = (alpha > 0.0f) == reverse ? -1.0f : 1.0f;
		mRecoverySteps = std::max(1, (int)std::round(recoveryTime / SimulationSettings::fixedStep));
		mStalledSteps = 0;
	}

	if (mRecoverySteps > 0)
	{
		mRecoverySteps--;
		action[0] = mRecoveryThrottle;
		action[1] = mRecoverySteering;

		// Після виїзду план складається наново, а не продовжує старий
		resetPlan();
		return;
	}

	ModelPredictiveBrain::decide(agent, observation, action);
}

// Подовжує горизонт, коли автомобіль майже стоїть
int CarPlannerBrain::getPlanningSteps(const float* state) const
{
	// Майже нерухомий автомобіль за звичайний горизонт ледь зрушить, тож план складається на довший час
	bool stalled = std::fabs(state[0]) < 2.0f;

	return std::max(1, horizon) * (stalled ? std::max(1, stallHorizonScale) : 1);
}

// Майже нерухомий автомобіль тримає дію з набору половину горизонту
int CarPlannerBrain::getHoldSteps(const float* state, int steps) const
{
	// Так серед кандидатів є розворот заднім ходом з повним кермом
	if (std::fabs(state[0]) < 2.0f) return std::max(1, steps / 2);

	return ModelPredictiveBrain::getHoldSteps(state, steps);
}

// Готує маршрут попереду, допустимі швидкості, перешкоди й прогнози рухомих об'єктів
void CarPlannerBrain::beginPlanning(Agent& agent, const float* state, float x, float z, float heading, int candidates, int steps)
{
	const CarAgent& car = static_cast<const CarAgent&>(agent);
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();
	float dt = SimulationSettings::fixedStep;

	mSteps = steps;
	mMaxSteering = vehicle->maxSteering > 0.0f ? vehicle->maxSteering : 0.3f;

	prepareRoute(car, x, z, heading);
	computeSpeedProfile(vehicle->maxSpeed > 0.0f ? vehicle->maxSpeed : 70.0f);

	// Власне віяло променів планувальника густіше за сенсор агента: вузьку перешкоду видно здалеку
	mObstacles.clear();

	Vector3 position, right, up, forward;
	vehicle->getBody()->getPose(position, right, up, forward);

	Vector3 origin = position + forward * 2.0f + up * 0.3f;
	int rays = std::max(1, scanRays);

	for (int i = 0; i < rays; i++)
	{
		float t = rays > 1 ? (float)i / (float)(rays - 1) - 0.5f : 0.0f;
		float angle = t * scanAngle * 3.14159265f / 180.0f;
		Vector3 direction = forward * std::cos(angle) + right * std::sin(angle);

		RaycastHit hit;

		if (!Physics::raycast(origin, direction, scanRange, hit, vehicle->getBody())) continue;

		// Стіни тримає на відстані коридор; перешкодою вважається лише те, що стоїть на самій дорозі
		if (distanceToRoute(hit.point.x, hit.point.z) > corridor + 1.5f) continue;

		mObstacles.push_back(hit.point.x);
		mObstacles.push_back(hit.point.z);
	}

	// Положення рухомих об'єктів на кожному кроці прогнозу обчислюються один раз для всіх кандидатів
	mTracked = car.getTrackedObjects(mTrackedPositions, mTrackedVelocities, mTrackedBodies);

	mObjectX.assign((size_t)std::max(mTracked, 1) * steps, 0.0f);
	mObjectZ.assign(mObjectX.size(), 0.0f);

	for (int o = 0; o < mTracked; o++)
	{
		for (int h = 0; h < steps; h++) objectAt(o, (h + 1) * dt, mObjectX[o * steps + h], mObjectZ[o * steps + h]);
	}

	// Сам агент може бути на будь-якому відрізку вікна, тож для нього шукаємо по всьому вікну
	int startSegment = 0;
	float deviation = 0.0f;
	mStartAlong = projectOnRoute(x, z, startSegment, deviation, (int)mRouteDistance.size());

	mSegments.assign(candidates, startSegment);
	mAlong.assign(candidates, mStartAlong);
}

// Після утримання дії з набору веде автомобіль маршрутом методом переслідування точки
void CarPlannerBrain::continueAction(int candidate, int step, const float* state, float x, float z, float heading, float* action)
{
	float speed = state[0];
	float along = mAlong[candidate];

	// Кут повороту коліс, за якого автомобіль піде дугою через точку на відстані упередження
	float lookahead = std::max(6.0f, 0.8f * std::fabs(speed));
	float tx, tz;
	pointOnRoute(along + lookahead, tx, tz);

	float dx = tx - x;
	float dz = tz - z;
	float alpha = std::atan2(dx, dz) - heading;

	while (alpha > 3.14159265f) alpha -= 6.2831853f;
	while (alpha < -3.14159265f) alpha += 6.2831853f;

	float distance = std::max(1.0f, std::sqrt(dx * dx + dz * dz));
	float wheelAngle = std::atan(2.0f * wheelBase * std::sin(alpha) / distance);

	action[1] = clampAction(wheelAngle / mMaxSteering);

	// Газ пропорційний різниці між допустимою й поточною швидкістю
	action[0] = clampAction(0.3f * (allowedSpeed(mSegments[candidate], along) - speed));
}

// Штрафує вихід з коридору, перешкоди, перевищення швидкості й ривки кермом
float CarPlannerBrain::evaluateStep(int candidate, int step, const float* state, const float* action, const float* previousAction, float x, float z, float heading)
{
	float deviation = 0.0f;
	mAlong[candidate] = projectOnRoute(x, z, mSegments[candidate], deviation);

	// Вихід за межі коридору навколо осі маршруту
	float outside = std::max(0.0f, deviation - corridor);
	float penalty = corridorWeight * outside * outside + centerWeight * deviation * deviation;

	// Наближення до стін і перешкод, які бачать промені
	for (size_t o = 0; o < mObstacles.size(); o += 2)
	{
		float dx = x - mObstacles[o];
		float dz = z - mObstacles[o + 1];
		float gap = obstacleRadius - std::sqrt(dx * dx + dz * dz);

		if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
	}

	// Наближення до прогнозованих положень рухомих об'єктів
	for (int o = 0; o < mTracked; o++)
	{
		float dx = x - mObjectX[o * mSteps + step];
		float dz = z - mObjectZ[o * mSteps + step];
		float gap = obstacleRadius * 1.5f - std::sqrt(dx * dx + dz * dz);

		if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
	}

	// Перевищення швидкості, з якої автомобіль ще встигає вписатися в повороти попереду
	float excess = std::max(0.0f, state[0] - allowedSpeed(mSegments[candidate], mAlong[candidate]));
	penalty += speedWeight * excess * excess;

	// Ривки кермом
	penalty += smoothWeight * std::fabs(action[1] - previousAction[1]);

	return -penalty;
}

// Додає просування маршрутом і штраф за перешкоди на продовженні шляху
float CarPlannerBrain::evaluateEnd(int candidate, const float* state, float x, float z, float heading)
{
	float dt = SimulationSettings::fixedStep;
	float score = mAlong[candidate] - mStartAlong;

	// Кінець шляху продовжується прямою з кінцевою швидкістю: шлях, що веде просто в перешкоду, видно раніше, ніж горизонт до неї дотягнеться
	float terminalStep = std::max(0.0f, terminalTime) / TERMINAL_POINTS;

	if (terminalStep <= 0.0f) return score;

	// Кожна точка продовження стоїть за кілька кроків прогнозу, тож і важить відповідно
	float terminalScale = terminalStep / dt;
	float speed = std::max(0.0f, state[0]);
	float dx = std::sin(heading) * speed;
	float dz = std::cos(heading) * speed;

	for (int p = 1; p <= TERMINAL_POINTS; p++)
	{
		float time = mSteps * dt + p * terminalStep;

		score -= obstaclePenalty(x + dx * p * terminalStep, z + dz * p * terminalStep, time) * terminalScale;
	}

	return score;
}

// Повертає штраф за наближення точки до перешкод і рухомих об'єктів у момент time
float CarPlannerBrain::obstaclePenalty(float x, float z, float time) const
{
	float penalty = 0.0f;

	for (size_t o = 0; o < mObstacles.size(); o += 2)
	{
		float dx = x - mObstacles[o];
		float dz = z - mObstacles[o + 1];
		float gap = obstacleRadius - std::sqrt(dx * dx + dz * dz);

		if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
	}

	for (int o = 0; o < mTracked; o++)
	{
		float objectX, objectZ;
		objectAt(o, time, objectX, objectZ);

		float dx = x - objectX;
		float dz = z - objectZ;
		float gap = obstacleRadius * 1.5f - std::sqrt(dx * dx + dz * dz);

		if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
	}

	return penalty;
}

// Записує прогнозоване положення рухомого об'єкта через time секунд
void CarPlannerBrain::objectAt(int object, float time, float& x, float& z) const
{
	// Прогнозувальник траєкторій сцени, якщо він є, передбачає рух точніше за сталу швидкість
	TrajectoryPredictor* predictor = TrajectoryPredictor::getActive();
	Vector3 forecast;

	if (predictor && mTrackedBodies[object] && predictor->predict(mTrackedBodies[object], time, forecast))
	{
		x = forecast.x;
		z = forecast.z;
		return;
	}

	x = mTrackedPositions[object].x + mTrackedVelocities[object].x * time;
	z = mTrackedPositions[object].z + mTrackedVelocities[object].z * time;
}

// Готує ламану маршруту попереду агента для оцінки просування
void CarPlannerBrain::prepareRoute(const CarAgent& agent, float x, float z, float heading)
{
	mRoute.clear();
	mRouteDistance.clear();

	const RouteTracker& tracker = agent.getRouteTracker();
	const std::vector<Vector3>& waypoints = tracker.getWaypoints();

	// Вікно починається на відрізок раніше: агент міг уже перейти на наступний відрізок, коли ще не доїхав до його початку
	int start = std::max(0, std::min(tracker.getSegment() - 1, (int)waypoints.size() - 1));

	// Без маршруту просування рахується вздовж поточного курсу
	if (waypoints.size() < 2)
	{
		mRoute = { x, z, x + std::sin(heading) * ROUTE_WINDOW, z + std::cos(heading) * ROUTE_WINDOW };
		mRouteDistance = { 0.0f, ROUTE_WINDOW };
		return;
	}

	float distance = 0.0f;

	for (int i = start; i < (int)waypoints.size(); i++)
	{
		if (!mRoute.empty())
		{
			float dx = waypoints[i].x - mRoute[mRoute.size() - 2];
			float dz = waypoints[i].z - mRoute[mRoute.size() - 1];
			distance += std::sqrt(dx * dx + dz * dz);
		}

		mRoute.push_back(waypoints[i].x);
		mRoute.push_back(waypoints[i].z);
		mRouteDistance.push_back(distance);

		if (distance > ROUTE_WINDOW && mRouteDistance.size() >= 2) break;
	}

	// Кінець маршруту подовжується прямою, щоб біля цілі було куди просуватися
	if (mRouteDistance.size() < 2)
	{
		mRoute.push_back(mRoute[0] + std::sin(heading));
		mRoute.push_back(mRoute[1] + std::cos(heading));
		mRouteDistance.push_back(1.0f);
	}
}

// Знаходить точку ламаної маршруту на відстані along від її початку
void CarPlannerBrain::pointOnRoute(float along, float& x, float& z) const
{
	int last = (int)mRouteDistance.size() - 1;
	int j = 0;

	while (j < last - 1 && along > mRouteDistance[j + 1]) j++;

	float length = mRouteDistance[j + 1] - mRouteDistance[j];
	float t = length > 0.0f ? (along - mRouteDistance[j]) / length : 0.0f;

	// За кінцем ламаної точка продовжує останній відрізок
	if (j < last - 1) t = std::min(t, 1.0f);

	x = mRoute[j * 2] + (mRoute[j * 2 + 2] - mRoute[j * 2]) * t;
	z = mRoute[j * 2 + 1] + (mRoute[j * 2 + 3] - mRoute[j * 2 + 1]) * t;
}

// Повертає відстань від точки до найближчого відрізка ламаної маршруту
float CarPlannerBrain::distanceToRoute(float x, float z) const
{
	float best = 1e9f;

	for (int j = 0; j + 1 < (int)mRouteDistance.size(); j++)
	{
		float ax = mRoute[j * 2], az = mRoute[j * 2 + 1];
		float sx = mRoute[j * 2 + 2] - ax, sz = mRoute[j * 2 + 3] - az;
		float lengthSquared = sx * sx + sz * sz;
		float t = lengthSquared > 0.0f ? std::max(0.0f, std::min(((x - ax) * sx + (z - az) * sz) / lengthSquared, 1.0f)) : 0.0f;
		float px = ax + sx * t - x, pz = az + sz * t - z;

		best = std::min(best, std::sqrt(px * px + pz * pz));
	}

	return best;
}

// Обчислює допустиму швидкість у точках ламаної: обмеження кривиною та гальмуванням перед наступними поворотами
void CarPlannerBrain::computeSpeedProfile(float maxSpeed)
{
	int points = (int)mRouteDistance.size();

	mRouteSpeed.assign(points, maxSpeed);

	// Кривина у вершині - кут повороту ламаної, поділений на середню довжину сусідніх відрізків
	for (int i = 1; i + 1 < points; i++)
	{
		float ax = mRoute[i * 2] - mRoute[i * 2 - 2], az = mRoute[i * 2 + 1] - mRoute[i * 2 - 1];
		float bx = mRoute[i * 2 + 2] - mRoute[i * 2], bz = mRoute[i * 2 + 3] - mRoute[i * 2 + 1];
		float lengthA = std::sqrt(ax * ax + az * az), lengthB = std::sqrt(bx * bx + bz * bz);

		if (lengthA < 1e-3f || lengthB < 1e-3f) continue;

		float cosine = std::max(-1.0f, std::min((ax * bx + az * bz) / (lengthA * lengthB), 1.0f));
		float curvature = std::acos(cosine) / (0.5f * (lengthA + lengthB));

		if (curvature > 1e-4f) mRouteSpeed[i] = std::min(maxSpeed, std::sqrt(lateralAcceleration / curvature));
	}

	// Зворотний прохід: швидкість у точці не може бути більшою, ніж та, з якої встигнеш загальмувати до наступної
	for (int i = points - 2; i >= 0; i--)
	{
		float length = mRouteDistance[i + 1] - mRouteDistance[i];
		float reachable = std::sqrt(mRouteSpeed[i + 1] * mRouteSpeed[i + 1] + 2.0f * braking * length);

		mRouteSpeed[i] = std::min(mRouteSpeed[i], reachable);
	}
}

// Повертає допустиму швидкість на відрізку segment на відстані along від початку ламаної
float CarPlannerBrain::allowedSpeed(int segment, float along) const
{
	int last = (int)mRouteSpeed.size() - 1;

	if (last < 1) return 1e9f;

	segment = std::max(0, std::min(segment, last - 1));

	float length = mRouteDistance[segment + 1] - mRouteDistance[segment];
	float t = length > 0.0f ? std::max(0.0f, std::min((along - mRouteDistance[segment]) / length, 1.0f)) : 0.0f;

	return mRouteSpeed[segment] + (mRouteSpeed[segment + 1] - mRouteSpeed[segment]) * t;
}

// Проєктує точку на ламану маршруту серед span відрізків, починаючи з segment; повертає пройдену відстань і відхилення
float CarPlannerBrain::projectOnRoute(float x, float z, int& segment, float& deviation, int span) const
{
	int segments = (int)mRouteDistance.size() - 1;
	float bestDistance = 1e9f;
	float bestAlong = 0.0f;
	int bestSegment = segment;

	// Прогнозований шлях шукаємо лише на поточному й двох наступних відрізках: так він не перескочить крізь стіну на сусідню ділянку маршруту
	for (int j = std::max(0, segment); j < std::min(segments, segment + span); j++)
	{
		float ax = mRoute[j * 2], az = mRoute[j * 2 + 1];
		float bx = mRoute[j * 2 + 2], bz = mRoute[j * 2 + 3];
		float sx = bx - ax, sz = bz - az;
		float lengthSquared = sx * sx + sz * sz;
		float t = lengthSquared > 0.0f ? ((x - ax) * sx + (z - az) * sz) / lengthSquared : 0.0f;

		// Крайні відрізки продовжуються за межі вікна, тож рух до його початку чи за його кінець теж рахується просуванням
		if (j > 0) t = std::max(0.0f, t);
		if (j < segments - 1) t = std::min(t, 1.0f);

		float px = ax + sx * t - x;
		float pz = az + sz * t - z;
		float distance = std::sqrt(px * px + pz * pz);

		if (distance < bestDistance)
		{
			bestDistance = distance;
			bestAlong = mRouteDistance[j] + t * std::sqrt(lengthSquared);
			bestSegment = j;
		}
	}

	segment = bestSegment;
	deviation = bestDistance;

	return bestAlong;
}

// Перелічує власні поля для файлу сцени та інспектора
void CarPlannerBrain::visitProperties(PropertyVisitor& visitor)
{
	ModelPredictiveBrain::visitProperties(visitor);

	visitor.property("wheelBase", wheelBase, 0.05f);
	visitor.property("corridor", corridor, 0.05f);
	visitor.property("corridorWeight", corridorWeight, 0.05f);
	visitor.property("centerWeight", centerWeight, 0.005f);
	visitor.property("obstacleRadius", obstacleRadius, 0.05f);
	visitor.property("obstacleWeight", obstacleWeight, 0.1f);
	visitor.property("smoothWeight", smoothWeight, 0.005f);
	visitor.property("lateralAcceleration", lateralAcceleration, 0.1f);
	visitor.property("braking", braking, 0.1f);
	visitor.property("speedWeight", speedWeight, 0.05f);
	visitor.property("scanRays", scanRays);
	visitor.property("scanAngle", scanAngle, 1.0f);
	visitor.property("scanRange", scanRange, 0.5f);
	visitor.property("terminalTime", terminalTime, 0.05f);
	visitor.property("stallHorizonScale", stallHorizonScale);
	visitor.property("recoveryDelay", recoveryDelay, 0.05f);
	visitor.property("recoveryTime", recoveryTime, 0.05f);

	stallHorizonScale = std::max(1, std::min(stallHorizonScale, 10));
}
