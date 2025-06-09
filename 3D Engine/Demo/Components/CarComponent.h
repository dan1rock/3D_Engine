#pragma once
#include "Component.h"
#include "Vector3.h"

class RigidBody;
class Prefab;
class Transform;

class CarComponent : public Component
{
public:
	CarComponent();
	~CarComponent();

	float force = 1.0f;
	float damping = 0.1f;
	float maxDistance = 1.0f;

	Prefab* wheelPrefab = nullptr;

protected:
	CarComponent* instantiate() const override
	{
		return new CarComponent(*this);
	}

private:
	struct wheel
	{
		Transform* transform;
		Vector3 position;
		bool rightSide;
		float steering;
		float spin;
		float spinSpeed;
		float acceleration;
		float springDistance;
	};

	void awake() override;
	void update() override;
	void fixedUpdate() override;

	void simulateWheel(wheel& wheel);

	RigidBody* mRigidBody = nullptr;

	wheel mWheelFR = {};
	wheel mWheelFL = {};
	wheel mWheelBR = {};
	wheel mWheelBL = {};

	float mSteering = 0.0f;
	float mSpeed = 0.0f;
};

