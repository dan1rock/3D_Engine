#pragma once
#include "Component.h"
#include "Vector3.h"

class RigidBody;

class LevitationTest : public Component
{
public:
	LevitationTest();
	~LevitationTest();

	float force = 1.0f;
	float damping = 0.1f;
	float maxDistance = 1.0f;

protected:
	LevitationTest* instantiate() const override
	{
		return new LevitationTest(*this);
	}

private:
	void awake() override;
	void fixedUpdate() override;

	void simulateSpring(Vector3 position);

	RigidBody* mRigidBody = nullptr;

};

