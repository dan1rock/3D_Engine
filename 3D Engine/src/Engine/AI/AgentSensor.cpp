#include "AgentSensor.h"
#include "RaySensor.h"
#include "VehicleController.h"
#include "Entity.h"
#include "RigidBody.h"
#include "EntityManager.h"
#include "Properties.h"
#include <cmath>
#include <algorithm>

// Обмежує значення спостереження проміжком [-1; 1]
static float clampUnit(float value)
{
	return std::max(-1.0f, std::min(value, 1.0f));
}

// Повертає відстань між точками в горизонтальній площині
static float horizontalDistance(const Vector3& a, const Vector3& b)
{
	float dx = a.x - b.x;
	float dz = a.z - b.z;
	return std::sqrt(dx * dx + dz * dz);
}

// Оновлює просування маршрутом і записує в out OBSERVATION_SIZE нормованих значень
void AgentSensor::observe(float* out)
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
	RigidBody* body = getOwner()->getComponent<RigidBody>();

	float maxSpeed = vehicle && vehicle->maxSpeed > 0.0f ? vehicle->maxSpeed : 70.0f;
	Vector3 velocity = body ? body->getVelocity() : Vector3();
	Vector3 angular = body ? body->getAngularVelocity() : Vector3();

	out[9] = clampUnit(velocity * forward / maxSpeed);
	out[10] = clampUnit(velocity * right / maxSpeed);
	out[11] = clampUnit(angular * up / maxYawRate);
	out[12] = vehicle && vehicle->maxSteering > 0.0f ? clampUnit(vehicle->getSteering() / vehicle->maxSteering) : 0.0f;

	// Напрям і відстань до точки маршруту попереду агента
	updateRoute(position);

	if (!mWaypoints.empty())
	{
		Vector3 target = mTarget;
		Vector3 toTarget = target - position;

		float x = toTarget * right;
		float z = toTarget * forward;
		float angle = std::atan2(x, z);

		out[13] = std::sin(angle);
		out[14] = std::cos(angle);
		out[15] = clampUnit(horizontalDistance(target, position) / range);
	}
	else
	{
		out[14] = 1.0f;
	}

	// Найближчі рухомі об'єкти
	observeObjects(position, velocity, out + 16);
}

// Починає маршрут заново з відрізка, що виходить із точки startWaypoint
void AgentSensor::resetRoute(int startWaypoint)
{
	mSegment = std::max(0, startWaypoint);
	mComplete = false;
	mRemaining = 0.0f;
}

// Повертає довжину решти маршруту від агента до цілі, обчислену останнім observe
float AgentSensor::getRemainingDistance() const
{
	return mRemaining;
}

// Перевіряє, чи агент дістався кінця маршруту
bool AgentSensor::isRouteComplete() const
{
	return mComplete;
}

// Повертає номер точки маршруту, до якої агент зараз прямує
int AgentSensor::getTargetWaypoint() const
{
	if (mWaypoints.empty()) return 0;

	return std::min(mSegment + 1, (int)mWaypoints.size() - 1);
}

// Повертає точки маршруту, зібрані останнім observe
const std::vector<Vector3>& AgentSensor::getWaypoints() const
{
	return mWaypoints;
}

// Повертає номер відрізка маршруту, на якому зараз агент
int AgentSensor::getSegment() const
{
	return mSegment;
}

// Повертає кількість найближчих рухомих об'єктів з останнього observe і записує їхні положення та швидкості
int AgentSensor::getTrackedObjects(Vector3* positions, Vector3* velocities) const
{
	for (int i = 0; i < mTrackedCount; i++)
	{
		positions[i] = mTrackedPositions[i];
		velocities[i] = mTrackedVelocities[i];
	}

	return mTrackedCount;
}

// Збирає точки маршруту з дочірніх об'єктів маршруту
void AgentSensor::collectWaypoints(Entity* route, std::vector<Vector3>& out)
{
	out.clear();

	if (!route) return;

	for (Entity* child : *route->getChildren())
	{
		out.push_back(child->getTransform()->getPosition());
	}
}

// Збирає маршрути: сам об'єкт, якщо його дочірні - точки, або кожен його дочірній об'єкт, що має власні точки
void AgentSensor::collectRoutes(Entity* root, std::vector<Entity*>& out)
{
	out.clear();

	if (!root) return;

	// Об'єкт, у якого хоч один дочірній має власних дочірніх, вважається групою маршрутів
	bool group = false;

	for (Entity* child : *root->getChildren())
	{
		if (!child->getChildren()->empty()) group = true;
	}

	if (!group)
	{
		out.push_back(root);
		return;
	}

	// Вимкнений маршрут групи до навчання не береться
	for (Entity* child : *root->getChildren())
	{
		if (child->isActiveSelf && child->getChildren()->size() >= 2) out.push_back(child);
	}
}

// Оновлює поточний відрізок маршруту й довжину решти маршруту за положенням агента
void AgentSensor::updateRoute(const Vector3& position)
{
	collectWaypoints(route, mWaypoints);

	int count = (int)mWaypoints.size();

	if (count < 2)
	{
		mSegment = 0;
		mRemaining = count == 1 ? horizontalDistance(mWaypoints[0], position) : 0.0f;
		mComplete = count == 1 && mRemaining < goalRadius;
		mTarget = count == 1 ? mWaypoints[0] : position;
		return;
	}

	mSegment = std::min(mSegment, count - 2);

	// Агент переходить на наступний відрізок, щойно проєкція його положення минає кінець поточного
	float t = 0.0f;

	while (true)
	{
		Vector3 a = mWaypoints[mSegment];
		Vector3 b = mWaypoints[mSegment + 1];

		float sx = b.x - a.x;
		float sz = b.z - a.z;
		float lengthSquared = sx * sx + sz * sz;

		t = lengthSquared > 0.0f ? ((position.x - a.x) * sx + (position.z - a.z) * sz) / lengthSquared : 1.0f;

		if (t >= 1.0f && mSegment < count - 2)
		{
			mSegment++;
			continue;
		}

		break;
	}

	t = std::max(0.0f, std::min(t, 1.0f));

	// Решта маршруту - недойдена частина поточного відрізка та всі наступні відрізки
	float remaining = (1.0f - t) * horizontalDistance(mWaypoints[mSegment], mWaypoints[mSegment + 1]);

	for (int i = mSegment + 2; i < count; i++)
	{
		remaining += horizontalDistance(mWaypoints[i - 1], mWaypoints[i]);
	}

	mRemaining = remaining;

	// Ціль - точка маршруту на відстані lookahead попереду проєкції агента, тож вона не залежить від того, як часто стоять точки
	Vector3 a = mWaypoints[mSegment];
	Vector3 b = mWaypoints[mSegment + 1];
	float ahead = lookahead + t * horizontalDistance(a, b);
	int segment = mSegment;

	while (true)
	{
		float length = horizontalDistance(mWaypoints[segment], mWaypoints[segment + 1]);

		if (ahead <= length || segment == count - 2)
		{
			float k = length > 0.0f ? std::min(ahead / length, 1.0f) : 1.0f;
			mTarget = mWaypoints[segment] + (mWaypoints[segment + 1] - mWaypoints[segment]) * k;
			break;
		}

		ahead -= length;
		segment++;
	}

	if (mSegment == count - 2 && (t >= 1.0f || horizontalDistance(mWaypoints[count - 1], position) < goalRadius))
	{
		mComplete = true;
	}
}

// Записує положення двох найближчих рухомих об'єктів зараз і через predictionTime
void AgentSensor::observeObjects(const Vector3& position, const Vector3& velocity, float* out)
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

		// Агентів своєї навчальної групи агент не бачить, бо й не може з ними зіткнутися
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

	// Відсутній об'єкт лишає нулі як у поточному, так і в прогнозованому положенні
	for (int i = 0; i < TRACKED_OBJECTS; i++)
	{
		RigidBody* body = nearest[i].second;

		if (!body) continue;

		mTrackedPositions[mTrackedCount] = body->getOwner()->getTransform()->getPosition();
		mTrackedVelocities[mTrackedCount] = body->getVelocity();
		mTrackedCount++;

		Vector3 relative = body->getOwner()->getTransform()->getPosition() - position;
		Vector3 predicted = relative + (body->getVelocity() - velocity) * predictionTime;

		out[i * 2] = clampUnit(relative * right / range);
		out[i * 2 + 1] = clampUnit(relative * forward / range);
		out[TRACKED_OBJECTS * 2 + i * 2] = clampUnit(predicted * right / range);
		out[TRACKED_OBJECTS * 2 + i * 2 + 1] = clampUnit(predicted * forward / range);
	}
}

// Перелічує власні поля для файлу сцени та інспектора
void AgentSensor::visitProperties(PropertyVisitor& visitor)
{
	visitor.reference("route", route);
	visitor.property("range", range, 0.5f);
	visitor.property("maxYawRate", maxYawRate, 0.05f);
	visitor.property("predictionTime", predictionTime, 0.05f);
	visitor.property("goalRadius", goalRadius, 0.1f);
	visitor.property("lookahead", lookahead, 0.1f);

	if (range < 0.01f) range = 0.01f;
	if (maxYawRate < 0.01f) maxYawRate = 0.01f;
}
