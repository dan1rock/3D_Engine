#include "PlaneCollider.h"
#include <PxPhysicsAPI.h>

PlaneCollider::PlaneCollider()
{
}

PlaneCollider::~PlaneCollider()
{
}

void* PlaneCollider::getGeometry(Vector3& scale, bool convex)
{
	if (mGeometry) {
		return mGeometry;
	}

	mGeometry = new physx::PxPlaneGeometry();

	return mGeometry;
}
