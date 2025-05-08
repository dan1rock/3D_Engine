#include "LevitationTest.h"
#include "RigidBody.h"
#include "Physics.h"
#include "GameObject.h"

LevitationTest::LevitationTest()
{
}

LevitationTest::~LevitationTest()
{
}

void LevitationTest::awake()
{
	mRigidBody = getOwner()->getComponent<RigidBody>();
}

void LevitationTest::fixedUpdate()
{
	if (!mRigidBody) return;

	Vector3 origin = mRigidBody->getOwner()->getTransform()->getPosition();
	Vector3 direction = Vector3(0, -1, 0);

	RaycastHit hit;
	if (Physics::raycast(origin, direction, maxDistance, hit, mRigidBody))
	{
		float velocity = mRigidBody->getVelocity() * -direction;
		float springForce = (maxDistance - hit.distance) / maxDistance * this->force - velocity * damping;
		mRigidBody->addForce(-direction * springForce);
	}
}
