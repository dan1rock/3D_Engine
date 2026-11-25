#include "RouteTracker.h"
#include "Entity.h"
#include <algorithm>
#include <cmath>

// Повертає відстань між точками в горизонтальній площині
static float horizontalDistance(const Vector3& a, const Vector3& b)
{
	float dx = a.x - b.x;
	float dz = a.z - b.z;
	return std::sqrt(dx * dx + dz * dz);
}

// Задає маршрут і починає його з відрізка, що виходить із точки startWaypoint
void RouteTracker::setRoute(Entity* route, int startWaypoint)
{
	mRoute = route;
	mSegment = std::max(0, startWaypoint);
	mComplete = false;
	mRemaining = 0.0f;
}

// Повертає поточний маршрут
Entity* RouteTracker::getRoute() const
{
	return mRoute;
}

// Оновлює просування за положенням об'єкта; точки перечитуються, бо їх можуть пересунути в редакторі
void RouteTracker::update(const Vector3& position)
{
	collectWaypoints(mRoute, mWaypoints);

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

	// Об'єкт переходить на наступний відрізок, щойно проєкція його положення минає кінець поточного
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
	mRemaining = (1.0f - t) * horizontalDistance(mWaypoints[mSegment], mWaypoints[mSegment + 1]) + pathLength(mWaypoints, mSegment + 1);

	// Ціль - точка маршруту на відстані lookahead попереду проєкції, тож вона не залежить від того, як часто стоять точки
	float ahead = lookahead + t * horizontalDistance(mWaypoints[mSegment], mWaypoints[mSegment + 1]);
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

// Повертає точки маршруту з останнього оновлення
const std::vector<Vector3>& RouteTracker::getWaypoints() const
{
	return mWaypoints;
}

// Повертає номер відрізка маршруту, на якому зараз об'єкт
int RouteTracker::getSegment() const
{
	return mSegment;
}

// Повертає довжину решти маршруту від об'єкта до кінця
float RouteTracker::getRemainingDistance() const
{
	return mRemaining;
}

// Повертає повну довжину маршруту
float RouteTracker::getLength() const
{
	return pathLength(mWaypoints, 0);
}

// Повертає частку пройденого маршруту від 0 до 1
float RouteTracker::getCompletion() const
{
	if (mComplete) return 1.0f;

	float total = getLength();

	return total > 0.0f ? std::max(0.0f, std::min(1.0f - mRemaining / total, 1.0f)) : 0.0f;
}

// Повертає точку маршруту на відстані lookahead попереду об'єкта
const Vector3& RouteTracker::getTarget() const
{
	return mTarget;
}

// Перевіряє, чи об'єкт дістався кінця маршруту
bool RouteTracker::isComplete() const
{
	return mComplete;
}

// Збирає точки маршруту з дочірніх об'єктів маршруту
void RouteTracker::collectWaypoints(Entity* route, std::vector<Vector3>& out)
{
	out.clear();

	if (!route) return;

	for (Entity* child : *route->getChildren())
	{
		out.push_back(child->getTransform()->getPosition());
	}
}

// Збирає маршрути: сам об'єкт, якщо його дочірні - точки, або кожен його увімкнений дочірній об'єкт, що має власні точки
void RouteTracker::collectRoutes(Entity* root, std::vector<Entity*>& out)
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

// Повертає довжину ламаної в горизонтальній площині від точки start до кінця
float RouteTracker::pathLength(const std::vector<Vector3>& waypoints, int start)
{
	float length = 0.0f;

	for (size_t i = std::max(start, 0) + 1; i < waypoints.size(); i++)
	{
		length += horizontalDistance(waypoints[i - 1], waypoints[i]);
	}

	return length;
}
