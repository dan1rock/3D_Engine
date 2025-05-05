#pragma once
#include "Component.h"
#include <PxPhysicsAPI.h>
#include "Vector3.h"

class RigidBody : public Component
{
public:
	RigidBody(bool isStatic = false);
    RigidBody(float radius, float mass, bool isStatic = false);
    ~RigidBody() override;

	void addForce(const Vector3& force);
	void setContinousCollisionDetection(bool ccd);

private:
	void awake() override;
	void update() override;
	void fixedUpdate() override;

	void updateShape();

    physx::PxRigidActor* mActor = nullptr;
	physx::PxMaterial* mMaterial = nullptr;
	physx::PxShape* mShape = nullptr;

	Vector3 mScale;

	float mRadius = 1.0f;
	float mMass = 1.0f;
	bool mIsStatic = false;
};

