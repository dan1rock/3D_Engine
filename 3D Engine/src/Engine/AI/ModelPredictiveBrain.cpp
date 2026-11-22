#include "ModelPredictiveBrain.h"
#include "DynamicsModel.h"
#include "LearnedDynamics.h"
#include "VehicleController.h"
#include "AgentSensor.h"
#include "RaySensor.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Entity.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

// Найбільша довжина ламаної маршруту попереду, яку враховує оцінка; її має вистачати, щоб загальмувати перед поворотом, м
static const float ROUTE_WINDOW = 150.0f;

// Обмежує дію проміжком [-1; 1]
static float clampAction(float value)
{
	return std::max(-1.0f, std::min(value, 1.0f));
}

// Розбирає набір рівнів дії з рядка чисел через кому чи пробіл; повторення й значення поза [-1; 1] відкидаються
static void parseLevels(const std::string& text, std::vector<float>& levels)
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
			value = clampAction(value);

			if (std::find(levels.begin(), levels.end(), value) == levels.end()) levels.push_back(value);
		}

		number.clear();
	}
}

// Виконує дію зі складеного плану, а коли настав час - складає новий
void ModelPredictiveBrain::decide(const float* observation, float* action)
{
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();
	AgentSensor* sensor = getOwner()->getComponent<AgentSensor>();
	const LearnedDynamics* model = DynamicsModel::loadShared(modelAsset);

	// Без моделі планувати нічим: агент стоїть
	if (!model || !vehicle || !sensor)
	{
		action[0] = 0.0f;
		action[1] = 0.0f;
		mPlannedPath.clear();
		mCandidatePaths.clear();
		return;
	}

	// Автомобіль, що застряг у позі, з якої жоден кандидат не веде вперед, виїжджає з неї, повертаючи ніс до маршруту
	float state[LearnedDynamics::STATE_SIZE];
	vehicle->getDynamicsState(state);

	mStalledSteps = std::fabs(state[0]) < 0.5f ? mStalledSteps + 1 : 0;

	if (mRecoverySteps == 0 && mStalledSteps * SimulationSettings::fixedStep >= recoveryDelay)
	{
		float x, z, heading;
		vehicle->getPlanarPose(x, z, heading);
		prepareRoute(sensor, x, z, heading);

		int segment = 0;
		float deviation = 0.0f;
		float along = projectOnRoute(x, z, segment, deviation, (int)mRouteDistance.size());

		float tx, tz;
		pointOnRoute(along + 8.0f, tx, tz);

		float alpha = std::atan2(tx - x, tz - z) - heading;

		while (alpha > 3.14159265f) alpha -= 6.2831853f;
		while (alpha < -3.14159265f) alpha += 6.2831853f;

		// Огляд навколо показує, де більше місця: позаду чи попереду
		float scan[LearnedDynamics::COLLISION_RAYS];
		DynamicsModel::scanSurroundings(vehicle, scan);

		int rays = LearnedDynamics::COLLISION_RAYS;
		float front = std::min(scan[0], std::min(scan[1], scan[rays - 1]));
		float back = std::min(scan[rays / 2], std::min(scan[rays / 2 - 1], scan[rays / 2 + 1]));

		// Задній хід з кермом убік, протилежний маршруту, розвертає ніс до маршруту; якщо позаду тісніше, автомобіль обережно повертає вперед
		bool reverse = back > front;
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
		mPlan.clear();
		return;
	}

	int interval = std::max(1, replanInterval);

	if (mPlan.empty() || mPlanStep >= interval || mPlanStep * 2 >= (int)mPlan.size())
	{
		plan(*model, vehicle, sensor);
	}

	action[0] = mPlan[mPlanStep * 2];
	action[1] = mPlan[mPlanStep * 2 + 1];
	mPlanStep++;
}

// Готує ламану маршруту попереду агента для оцінки просування
void ModelPredictiveBrain::prepareRoute(AgentSensor* sensor, float x, float z, float heading)
{
	mRoute.clear();
	mRouteDistance.clear();

	const std::vector<Vector3>& waypoints = sensor->getWaypoints();
	// Вікно починається на відрізок раніше: сенсор міг уже перейти на наступний відрізок, коли агент ще не доїхав до його початку
	int start = std::max(0, std::min(sensor->getSegment() - 1, (int)waypoints.size() - 1));

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
void ModelPredictiveBrain::pointOnRoute(float along, float& x, float& z) const
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

// Повертає дію простої стратегії слідування маршрутом: кермо на точку попереду, газ до допустимої швидкості
void ModelPredictiveBrain::followRoute(const float* state, float x, float z, float heading, int segment, float along, float maxSteering, float* action) const
{
	float speed = state[0];

	// Метод переслідування точки: кут повороту коліс, за якого автомобіль піде дугою через точку на відстані упередження
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

	action[1] = clampAction(wheelAngle / maxSteering);

	// Газ пропорційний різниці між допустимою й поточною швидкістю
	action[0] = clampAction(0.3f * (allowedSpeed(segment, along) - speed));
}

// Повертає відстань від точки до найближчого відрізка ламаної маршруту
float ModelPredictiveBrain::distanceToRoute(float x, float z) const
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
void ModelPredictiveBrain::computeSpeedProfile(float maxSpeed)
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
float ModelPredictiveBrain::allowedSpeed(int segment, float along) const
{
	int last = (int)mRouteSpeed.size() - 1;

	if (last < 1) return 1e9f;

	segment = std::max(0, std::min(segment, last - 1));

	float length = mRouteDistance[segment + 1] - mRouteDistance[segment];
	float t = length > 0.0f ? std::max(0.0f, std::min((along - mRouteDistance[segment]) / length, 1.0f)) : 0.0f;

	return mRouteSpeed[segment] + (mRouteSpeed[segment + 1] - mRouteSpeed[segment]) * t;
}

// Проєктує точку на ламану маршруту серед span відрізків, починаючи з segment; повертає пройдену відстань і відхилення
float ModelPredictiveBrain::projectOnRoute(float x, float z, int& segment, float& deviation, int span) const
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

// Складає новий план: перебирає поєднання дій із наборів і запам'ятовує найкраще
void ModelPredictiveBrain::plan(const LearnedDynamics& model, VehicleController* vehicle, AgentSensor* sensor)
{
	// Набори рівнів перечитуються лише тоді, коли їх змінили в інспекторі
	if (throttlePool != mParsedThrottlePool || mThrottleLevels.empty())
	{
		parseLevels(throttlePool, mThrottleLevels);
		if (mThrottleLevels.empty()) mThrottleLevels = { 0.0f, 0.5f, 1.0f };
		mParsedThrottlePool = throttlePool;
	}

	if (steeringPool != mParsedSteeringPool || mSteeringLevels.empty())
	{
		parseLevels(steeringPool, mSteeringLevels);
		if (mSteeringLevels.empty()) mSteeringLevels = { -1.0f, -0.5f, 0.0f, 0.5f, 1.0f };
		mParsedSteeringPool = steeringPool;
	}

	int count = (int)(mThrottleLevels.size() * mSteeringLevels.size());
	float dt = SimulationSettings::fixedStep;

	float state[LearnedDynamics::STATE_SIZE];
	float x0, z0, heading0;

	vehicle->getDynamicsState(state);

	// Майже нерухомий автомобіль за звичайний горизонт ледь зрушить, тож план складається на довший час
	bool stalled = std::fabs(state[0]) < 2.0f;
	int steps = std::max(1, horizon) * (stalled ? std::max(1, stallHorizonScale) : 1);
	vehicle->getPlanarPose(x0, z0, heading0);
	prepareRoute(sensor, x0, z0, heading0);
	computeSpeedProfile(vehicle->maxSpeed > 0.0f ? vehicle->maxSpeed : 70.0f);

	// Перешкоди: точки, де промені сенсора влучили в стіни й предмети, та прогнозовані положення рухомих об'єктів
	std::vector<float> obstacles;

	{
		// Власне віяло променів планувальника густіше за сенсор агента: вузьку перешкоду видно здалеку
		Vector3 position, right, up, forward;
		vehicle->getRigidBody()->getPose(position, right, up, forward);

		Vector3 origin = position + forward * 2.0f + up * 0.3f;
		int rays = std::max(1, scanRays);

		for (int i = 0; i < rays; i++)
		{
			float t = rays > 1 ? (float)i / (float)(rays - 1) - 0.5f : 0.0f;
			float angle = t * scanAngle * 3.14159265f / 180.0f;
			Vector3 direction = forward * std::cos(angle) + right * std::sin(angle);

			RaycastHit hit;

			if (!Physics::raycast(origin, direction, scanRange, hit, vehicle->getRigidBody())) continue;

			// Стіни тримає на відстані коридор; перешкодою вважається лише те, що стоїть на самій дорозі
			if (distanceToRoute(hit.point.x, hit.point.z) > corridor + 1.5f) continue;

			obstacles.push_back(hit.point.x);
			obstacles.push_back(hit.point.z);
		}
	}

	Vector3 trackedPositions[AgentSensor::TRACKED_OBJECTS];
	Vector3 trackedVelocities[AgentSensor::TRACKED_OBJECTS];
	int tracked = sensor->getTrackedObjects(trackedPositions, trackedVelocities);

	// Кандидати - усі поєднання рівнів газу й керма з наборів; дію з набору кандидат тримає holdTime, а далі дії додаються під час прогнозу
	mSequences.assign((size_t)count * steps * 2, 0.0f);

	// Майже нерухомий автомобіль тримає дію з набору половину довгого горизонту: так серед кандидатів є розворот заднім ходом з повним кермом
	int holdSteps = stalled ? std::max(1, steps / 2) : std::max(1, std::min(steps, (int)std::round(holdTime / dt)));
	float maxSteering = vehicle->maxSteering > 0.0f ? vehicle->maxSteering : 0.3f;

	for (int k = 0; k < count; k++)
	{
		float throttle = mThrottleLevels[k / mSteeringLevels.size()];
		float steering = mSteeringLevels[k % mSteeringLevels.size()];
		float* sequence = mSequences.data() + (size_t)k * steps * 2;

		for (int h = 0; h < holdSteps; h++)
		{
			sequence[h * 2] = throttle;
			sequence[h * 2 + 1] = steering;
		}
	}

	// Усі кандидати прогнозуються разом: мережа обчислює їхні кроки одним пакетом
	mStates.resize((size_t)count * LearnedDynamics::STATE_SIZE);
	mNextStates.resize(mStates.size());
	mActions.resize((size_t)count * LearnedDynamics::ACTION_SIZE);
	mPositions.resize((size_t)count * steps * 2);

	std::vector<float> xs(count, x0), zs(count, z0), headings(count, heading0), scores(count, 0.0f);
	std::vector<int> segments(count, 0);

	int startSegment = 0;
	float deviation0 = 0.0f;
	// Сам агент може бути на будь-якому відрізку вікна, тож для нього шукаємо по всьому вікну
	float startAlong = projectOnRoute(x0, z0, startSegment, deviation0, (int)mRouteDistance.size());

	for (int k = 0; k < count; k++)
	{
		std::copy(state, state + LearnedDynamics::STATE_SIZE, mStates.data() + (size_t)k * LearnedDynamics::STATE_SIZE);
		segments[k] = startSegment;
	}

	std::vector<float> along(count, startAlong);
	float previousSteering = vehicle->getSteeringAction();

	for (int h = 0; h < steps; h++)
	{
		for (int k = 0; k < count; k++)
		{
			float* applied = mSequences.data() + ((size_t)k * steps + h) * 2;

			// Після утримання дії з набору прогноз продовжує стратегія слідування маршрутом: так видно, куди веде саме початковий вибір
			if (h >= holdSteps)
			{
				followRoute(mStates.data() + (size_t)k * LearnedDynamics::STATE_SIZE, xs[k], zs[k], headings[k], segments[k], along[k], maxSteering, applied);
			}

			mActions[k * 2] = applied[0];
			mActions[k * 2 + 1] = applied[1];
		}

		model.predictBatch(mStates.data(), mActions.data(), count, mNextStates.data());

		float time = (h + 1) * dt;

		for (int k = 0; k < count; k++)
		{
			const float* next = mNextStates.data() + (size_t)k * LearnedDynamics::STATE_SIZE;

			LearnedDynamics::integratePose(xs[k], zs[k], headings[k], next, dt);

			mPositions[((size_t)k * steps + h) * 2] = xs[k];
			mPositions[((size_t)k * steps + h) * 2 + 1] = zs[k];

			float deviation = 0.0f;
			along[k] = projectOnRoute(xs[k], zs[k], segments[k], deviation);

			float penalty = 0.0f;

			// Вихід за межі коридору навколо осі маршруту
			float outside = std::max(0.0f, deviation - corridor);
			penalty += corridorWeight * outside * outside + centerWeight * deviation * deviation;

			// Наближення до стін і перешкод, які бачать промені
			for (size_t o = 0; o < obstacles.size(); o += 2)
			{
				float dx = xs[k] - obstacles[o];
				float dz = zs[k] - obstacles[o + 1];
				float gap = obstacleRadius - std::sqrt(dx * dx + dz * dz);

				if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
			}

			// Наближення до прогнозованих положень рухомих об'єктів
			for (int o = 0; o < tracked; o++)
			{
				float dx = xs[k] - (trackedPositions[o].x + trackedVelocities[o].x * time);
				float dz = zs[k] - (trackedPositions[o].z + trackedVelocities[o].z * time);
				float gap = obstacleRadius * 1.5f - std::sqrt(dx * dx + dz * dz);

				if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
			}

			// Перевищення швидкості, з якої автомобіль ще встигає вписатися в повороти попереду
			float excess = std::max(0.0f, next[0] - allowedSpeed(segments[k], along[k]));
			penalty += speedWeight * excess * excess;

			// Ривки кермом
			float steering = mActions[k * 2 + 1];
			float before = h == 0 ? previousSteering : mSequences[((size_t)k * steps + h - 1) * 2 + 1];
			penalty += smoothWeight * std::fabs(steering - before);

			scores[k] -= penalty;
		}

		mStates.swap(mNextStates);
	}

	// Кінець шляху продовжується прямою з кінцевою швидкістю: шлях, що веде просто в перешкоду, видно раніше, ніж горизонт до неї дотягнеться
	const int terminalPoints = 10;
	float terminalStep = std::max(0.0f, terminalTime) / terminalPoints;
	float terminalScale = terminalStep / dt;

	for (int k = 0; k < count && terminalStep > 0.0f; k++)
	{
		const float* last = mStates.data() + (size_t)k * LearnedDynamics::STATE_SIZE;
		float speed = std::max(0.0f, last[0]);
		float dx = std::sin(headings[k]) * speed;
		float dz = std::cos(headings[k]) * speed;

		for (int p = 1; p <= terminalPoints; p++)
		{
			float time = steps * dt + p * terminalStep;
			float px = xs[k] + dx * p * terminalStep;
			float pz = zs[k] + dz * p * terminalStep;
			float penalty = 0.0f;

			for (size_t o = 0; o < obstacles.size(); o += 2)
			{
				float ox = px - obstacles[o];
				float oz = pz - obstacles[o + 1];
				float gap = obstacleRadius - std::sqrt(ox * ox + oz * oz);

				if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
			}

			for (int o = 0; o < tracked; o++)
			{
				float ox = px - (trackedPositions[o].x + trackedVelocities[o].x * time);
				float oz = pz - (trackedPositions[o].z + trackedVelocities[o].z * time);
				float gap = obstacleRadius * 1.5f - std::sqrt(ox * ox + oz * oz);

				if (gap > 0.0f) penalty += obstacleWeight * gap * gap;
			}

			// Кожна точка продовження стоїть за кілька кроків прогнозу, тож і важить відповідно
			scores[k] -= penalty * terminalScale;
		}
	}

	// Навчена модель зіткнень оцінює кожну дію з набору за тим, що зараз навколо автомобіля
	if (model.hasCollisionModel() && collisionWeight > 0.0f)
	{
		float scan[LearnedDynamics::COLLISION_RAYS];
		DynamicsModel::scanSurroundings(vehicle, scan);

		for (int k = 0; k < count; k++)
		{
			const float* action = mSequences.data() + (size_t)k * steps * 2;
			scores[k] -= collisionWeight * model.predictCollision(scan, state, action);
		}
	}

	// Оцінка - просування маршрутом за горизонт мінус накопичені штрафи
	int best = 0;

	for (int k = 0; k < count; k++)
	{
		scores[k] += along[k] - startAlong;

		if (scores[k] > scores[best]) best = k;
	}

	mPlan.assign(mSequences.begin() + (size_t)best * steps * 2, mSequences.begin() + (size_t)(best + 1) * steps * 2);
	mPlanStep = 0;

	// Шлях для малювання - на висоті корпусу
	float height = getOwner()->getTransform()->getPosition().y;

	mPlannedPath.clear();
	mPlannedPath.push_back(Vector3(x0, height, z0));

	for (int h = 0; h < steps; h++)
	{
		mPlannedPath.push_back(Vector3(mPositions[((size_t)best * steps + h) * 2], height, mPositions[((size_t)best * steps + h) * 2 + 1]));
	}

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

	for (size_t i = 0; i < order.size(); i++)
	{
		std::vector<Vector3>& path = mCandidatePaths[i];
		path.reserve(steps + 1);
		path.push_back(Vector3(x0, height, z0));

		for (int h = 0; h < steps; h++)
		{
			path.push_back(Vector3(mPositions[((size_t)order[i] * steps + h) * 2], height, mPositions[((size_t)order[i] * steps + h) * 2 + 1]));
		}
	}
}

// Малює вибраний прогнозований шлях
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
	AgentBrain::visitProperties(visitor);

	visitor.asset("modelAsset", modelAsset, LearnedDynamics::FILE_EXTENSION);
	visitor.property("throttlePool", throttlePool);
	visitor.property("steeringPool", steeringPool);
	visitor.property("holdTime", holdTime, 0.01f);
	visitor.property("wheelBase", wheelBase, 0.05f);
	visitor.property("horizon", horizon);
	visitor.property("replanInterval", replanInterval);
	visitor.property("corridor", corridor, 0.05f);
	visitor.property("corridorWeight", corridorWeight, 0.05f);
	visitor.property("centerWeight", centerWeight, 0.005f);
	visitor.property("obstacleRadius", obstacleRadius, 0.05f);
	visitor.property("obstacleWeight", obstacleWeight, 0.1f);
	visitor.property("collisionWeight", collisionWeight, 0.5f);
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
	visitor.property("showPlan", showPlan);
	visitor.property("showCandidates", showCandidates);

	horizon = std::max(1, std::min(horizon, 500));
	replanInterval = std::max(1, replanInterval);
	stallHorizonScale = std::max(1, std::min(stallHorizonScale, 10));
}
