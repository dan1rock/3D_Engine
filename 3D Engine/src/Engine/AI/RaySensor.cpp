#include "RaySensor.h"
#include "Entity.h"
#include "Physics.h"
#include "RigidBody.h"
#include "Properties.h"
#include <cmath>

// Випускає промені й записує в out rayCount відстаней, поділених на дальність: 1 означає, що перешкод немає
void RaySensor::sense(float* out)
{
	int count = rayCount > 0 ? rayCount : 0;

	mReadings.resize(count);

	Vector3 origin = getOrigin();

	// Власне тіло промені не бачать, а з ним і агентів тієї самої навчальної групи
	RigidBody* own = getOwner()->getComponent<RigidBody>();

	for (int i = 0; i < count; i++)
	{
		float reading = 1.0f;
		RaycastHit hit;

		if (range > 0.0f && Physics::raycast(origin, getRayDirection(i), range, hit, own))
		{
			reading = std::fmin(hit.distance, range) / range;
		}

		mReadings[i] = reading;

		if (out) out[i] = reading;
	}
}

// Повертає відстані, виміряні останнім викликом sense
const std::vector<float>& RaySensor::getReadings() const
{
	return mReadings;
}

// Повертає точку, з якої випускаються промені
Vector3 RaySensor::getOrigin()
{
	Transform* transform = getOwner()->getTransform();

	return transform->getPosition() + transform->getForward() * forwardOffset + transform->getUp() * heightOffset;
}

// Повертає напрям променя за номером
Vector3 RaySensor::getRayDirection(int index)
{
	Transform* transform = getOwner()->getTransform();

	// Промені рівномірно розходяться від -fieldOfView/2 ліворуч до fieldOfView/2 праворуч
	float t = rayCount > 1 ? (float)index / (float)(rayCount - 1) - 0.5f : 0.0f;
	float angle = t * fieldOfView * 3.14159265f / 180.0f;

	return transform->getForward() * std::cos(angle) + transform->getRight() * std::sin(angle);
}

// Перелічує власні поля для файлу сцени та інспектора
void RaySensor::visitProperties(PropertyVisitor& visitor)
{
	visitor.property("rayCount", rayCount);
	visitor.property("fieldOfView", fieldOfView, 1.0f);
	visitor.property("range", range, 0.5f);
	visitor.property("forwardOffset", forwardOffset, 0.05f);
	visitor.property("heightOffset", heightOffset, 0.05f);

	if (rayCount < 0) rayCount = 0;
}
