#include "LevitationTest.h"
#include "RigidBody.h"
#include "Physics.h"
#include "GameObject.h"

#include <iostream>

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

	float base = 1.0f;

	Transform* transform = mRigidBody->getOwner()->getTransform();

	simulateSpring(transform->getPosition() + (transform->getRight() + transform->getForward()) * base);
	simulateSpring(transform->getPosition() + (-transform->getRight() + transform->getForward()) * base);
	simulateSpring(transform->getPosition() + (-transform->getRight() - transform->getForward()) * base);
	simulateSpring(transform->getPosition() + (transform->getRight() - transform->getForward()) * base);
}

void LevitationTest::simulateSpring(Vector3 position)
{
	RaycastHit hit;
	Vector3 direction = -mRigidBody->getOwner()->getTransform()->getUp();
	if (Physics::raycast(position, direction, maxDistance, hit, mRigidBody))
	{
		float velocity = mRigidBody->getVelocityAtPoint(position) * -direction;
		float springForce = (maxDistance - hit.distance) / maxDistance * this->force - velocity * damping;
		mRigidBody->addForce(-direction * springForce, position);
	}
}
