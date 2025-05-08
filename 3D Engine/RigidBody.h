#pragma once
#include "Component.h"
#include <PxPhysicsAPI.h>
#include "Vector3.h"
#include <list>

class Collider;

class RigidBody : public Component
{
public:
	RigidBody(bool isStatic = false);
    RigidBody(float radius, float mass, bool isStatic = false);
    ~RigidBody() override;

	Vector3 getVelocity();
	Vector3 getVelocityAtPoint(const Vector3& point);
	Vector3 getAngularVelocity();

	void addForce(const Vector3& force);
	void addForce(const Vector3& force, const Vector3& position);
	void addTorque(const Vector3& torque);
	void setContinousCollisionDetection(bool ccd);

protected:
	RigidBody* instantiate() const override {
		return new RigidBody(*this);
	};

private:
	void awake() override;
	void update() override;
	void fixedUpdate() override;

	void updateShape();
	void releaseShapes();

	void updateGlobalPose();

	void addCollider(Collider* collider);

	std::list<Collider*> mColliders = {};

    physx::PxRigidActor* mActor = nullptr;
	physx::PxMaterial* mMaterial = nullptr;

	Vector3 mScale;

	float mRadius = 1.0f;
	float mMass = 1.0f;
	bool mIsStatic = false;
	bool mCcd = false;

	friend class Transform;
	friend class Collider;
	friend class ComponentManager;
	friend class Physics;
};

