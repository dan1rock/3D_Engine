#include "CarComponent.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Prefab.h"
#include "Input.h"
#include "EngineTime.h"
#include "imgui.h"

#include <iostream>

CarComponent::CarComponent()
{
}

CarComponent::~CarComponent()
{
}

void CarComponent::awake()
{
	mRigidBody = getOwner()->getComponent<RigidBody>();
	mRigidBody->setCenterOfMass(Vector3(0.0f, 0.3f, -0.1f));

	mWheelFR.transform = wheelPrefab->instantiate()->getTransform();
	mWheelFL.transform = wheelPrefab->instantiate()->getTransform();
	mWheelBR.transform = wheelPrefab->instantiate()->getTransform();
	mWheelBL.transform = wheelPrefab->instantiate()->getTransform();

	mWheelFR.rightSide = true;
	mWheelBR.rightSide = true;
}

void CarComponent::update()
{
	ImGui::Begin("Player Car", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::SliderFloat("Speed", &mSpeed, 0.0f, 100.0f);
	ImGui::End();
}

void CarComponent::fixedUpdate()
{
	if (!mRigidBody) return;

	float carSpeed = mRigidBody->getVelocity() * mOwner->getTransform()->getForward();
	float absoluteSpeed = carSpeed;
	if (absoluteSpeed < 0.0f) absoluteSpeed = -absoluteSpeed;
	mSpeed = absoluteSpeed;

	float accelerationRatio = 1.0f - absoluteSpeed / 70.0f;

	float targetSteering = 0.0f;

	if (Input::getKey('A'))
	{
		targetSteering += -0.5f;
	}
	if (Input::getKey('D'))
	{
		targetSteering += 0.5f;
	}

	mSteering += (targetSteering - mSteering) * 0.1f;

	float acceleration = 0.0f;

	if (Input::getKey('W'))
	{
		acceleration += 10.0f;
	}
	if (Input::getKey('S'))
	{
		acceleration += -10.0f;
	}

	if (carSpeed * acceleration >= 0.0f)
	{
		acceleration *= accelerationRatio;
	}

	mWheelFR.steering = mSteering;
	mWheelFL.steering = mSteering;

	mWheelBL.acceleration = acceleration;
	mWheelBR.acceleration = acceleration;

	float base = 0.85f;

	Transform* transform = getOwner()->getTransform();

	Vector3 basePos = transform->getPosition() + transform->getUp() * 0.1f;

	mWheelFR.position = basePos + transform->getRight() * base + transform->getForward() * base * 1.53f;
	mWheelFL.position = basePos - transform->getRight() * base + transform->getForward() * base * 1.53f;
	mWheelBR.position = basePos + transform->getRight() * base - transform->getForward() * base * 1.8f;
	mWheelBL.position = basePos - transform->getRight() * base - transform->getForward() * base * 1.8f;

	simulateWheel(mWheelFR);
	simulateWheel(mWheelFL);
	simulateWheel(mWheelBL);
	simulateWheel(mWheelBR);

	mRigidBody->addForce(Vector3(0, -9.81, 0));
}

void CarComponent::simulateWheel(wheel& wheel)
{
	Transform* transform = getOwner()->getTransform();

	RaycastHit hit;
	Vector3 direction = -getOwner()->getTransform()->getUp();
	Vector3 position = wheel.position;

	if (Physics::raycast(position + getOwner()->getTransform()->getUp() * 0.5f, direction, maxDistance + 0.5f, hit, mRigidBody))
	{
		hit.distance -= 0.5f;

		Vector3 forward = Vector3::rotateAroundUp(transform->getForward(), transform->getUp(), wheel.steering);

		// Grip

		Vector3 movementDir = mRigidBody->getVelocityAtPoint(position);
		Vector3 wheelForward = forward;

		if (movementDir.length() < 10.00f)
		{
			movementDir = forward;
		}

		movementDir.y = 0.0f;
		wheelForward.y = 0.0f;

		movementDir.normalize();
		wheelForward.normalize();

		float slip = movementDir * wheelForward;

		if (slip < 0.0f) slip = -slip;
		if (slip < 0.5f) slip = 0.5f;

		float grip = 3.0f * slip * slip;

		// Suspension
		float velocity = mRigidBody->getVelocityAtPoint(position) * -direction;
		float springForce = (maxDistance - hit.distance) / maxDistance * this->force - velocity * damping;
		mRigidBody->addForce(-direction * springForce, hit.point - direction * 0.3f);

		// Steering
		Vector3 up = Vector3(0, 1, 0);
		Vector3 lateral = up.cross(forward).normalized();

		Vector3 wheelVel = mRigidBody->getVelocityAtPoint(hit.point);
		float latSpeed = lateral * wheelVel;

		Vector3 frictionForce = -lateral * (latSpeed * grip);

		mRigidBody->addForce(frictionForce, hit.point);

		// Acceleration
		if (wheel.acceleration != 0.0f)
		{
			Vector3 accelerationForce = forward * wheel.acceleration;
			mRigidBody->addForce(accelerationForce, hit.point);
		}

		wheel.spinSpeed = wheelVel * forward * 0.04f;
		wheel.spin += wheel.spinSpeed;

		float maxDiff = 0.01f;
		float distanceDiff = (hit.distance - 0.3f) - wheel.springDistance;
		if (distanceDiff < -maxDiff) distanceDiff = -maxDiff;
		wheel.springDistance += distanceDiff;

		Matrix wheelMatrix = {};
		wheelMatrix.setIdentity();
		wheelMatrix.setTranslation(Vector3());
		wheelMatrix.setRotation(Vector3(wheel.spin * (wheel.rightSide ? -1.0f : 1.0f), wheel.steering + (wheel.rightSide ? 3.1415f : 0.0f), 0.0f));

		Matrix newMatrix = *transform->getMatrix();
		wheelMatrix *= newMatrix;

		wheel.transform->setMatrix(wheelMatrix);
		wheel.transform->setPosition(position + direction * wheel.springDistance);

		// Drag
		float wheelSpeed = wheelVel * forward;

		float dragForce = wheelSpeed;
		if (dragForce > 0.5f) dragForce = 0.5f;
		if (dragForce < -2.0f) dragForce = -2.0f;

		mRigidBody->addForce(-forward * dragForce, hit.point);
	}
	else
	{
		wheel.spin += wheel.spinSpeed;
		wheel.spinSpeed *= 0.995f;

		Matrix wheelMatrix = {};
		wheelMatrix.setIdentity();
		wheelMatrix.setRotation(Vector3(wheel.spin * (wheel.rightSide ? -1.0f : 1.0f), wheel.steering + (wheel.rightSide ? 3.1415f : 0.0f), 0.0f));

		Matrix newMatrix = *transform->getMatrix();
		wheelMatrix *= newMatrix;

		wheel.transform->setMatrix(wheelMatrix);

		float maxDiff = 0.01f;
		float distanceDiff = (maxDistance - 0.3f) - wheel.springDistance;
		if (distanceDiff < -maxDiff) distanceDiff = -maxDiff;
		wheel.springDistance += distanceDiff;

		wheel.transform->setPosition(position + direction * wheel.springDistance);
	}
}
