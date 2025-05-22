#include "PlaneCollider.h"
#include <PxPhysicsAPI.h>

PlaneCollider::PlaneCollider()
{
}

PlaneCollider::~PlaneCollider()
{
}

// Повертає вказівник на геометрію коллайдера
void* PlaneCollider::getGeometry(Vector3& scale, bool convex)
{
	if (mGeometry) {
		return mGeometry;
	}

	mGeometry = new physx::PxPlaneGeometry();

	return mGeometry;
}
