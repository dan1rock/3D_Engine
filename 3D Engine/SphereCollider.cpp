#include "SphereCollider.h"
#include <PxPhysicsAPI.h>

SphereCollider::SphereCollider()
{
}

SphereCollider::SphereCollider(float radius)
{
	this->radius = radius;
}

SphereCollider::~SphereCollider()
{
}

void* SphereCollider::getGeometry(Vector3& scale, bool convex)
{
	if (mGeometry) {
		if (scale == mScale) {
			return mGeometry;
		}

		delete mGeometry;
		mGeometry = nullptr;
	}

	mScale = scale;

	float scaledRadius = radius * scale.x;

	mGeometry = new physx::PxSphereGeometry(scaledRadius);

    return mGeometry;
}
