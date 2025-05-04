#pragma once
#include "Component.h"
#include <PxPhysicsAPI.h>

class RigidBody : public Component
{
public:
	RigidBody(bool isStatic = false);
    RigidBody(float radius, float mass, bool isStatic = false);
    ~RigidBody() override;

    void awake() override;
    void update() override;

private:
    physx::PxRigidActor* mActor = nullptr;

	float mRadius = 1.0f;
	float mMass = 1.0f;
	bool mIsStatic = false;
};

