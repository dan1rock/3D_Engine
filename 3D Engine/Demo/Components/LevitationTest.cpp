#include "LevitationTest.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Entity.h"
#include "Input.h"

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

	Transform* transform = getOwner()->getTransform();

	simulateSpring(transform->getPosition() + transform->getRight() * base + transform->getForward() * base * 2.0f);
	simulateSpring(transform->getPosition() + -transform->getRight() * base + transform->getForward() * base * 2.0f);
	simulateSpring(transform->getPosition() + -transform->getRight() * base - transform->getForward() * base * 2.0f);
	simulateSpring(transform->getPosition() + transform->getRight() * base - transform->getForward() * base * 2.0f);

	Vector3 point = transform->getPosition() - transform->getUp() * 0.2f;

	if (Input::getKey('Y'))
	{
		mRigidBody->addForce(transform->getForward() * 10.0f, point);
	}
	if (Input::getKey('H'))
	{
		mRigidBody->addForce(-transform->getForward() * 10.0f, point);
	}
}

void LevitationTest::simulateSpring(Vector3 position)
{
	RaycastHit hit;
	Vector3 direction = -getOwner()->getTransform()->getUp();
	if (Physics::raycast(position, direction, maxDistance, hit, mRigidBody))
	{
		float velocity = mRigidBody->getVelocityAtPoint(position) * -direction;
		float springForce = (maxDistance - hit.distance) / maxDistance * this->force - velocity * damping;
		mRigidBody->addForce(-direction * springForce, position);
	}
}
