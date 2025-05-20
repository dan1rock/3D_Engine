#include "RigidBody.h"
#include "PhysicsEngine.h"
#include "ConvexMeshManager.h"
#include "GameObject.h"
#include "Quaternion.h"
#include "Collider.h"
#include "EntityManager.h"

using namespace physx;

RigidBody::RigidBody(bool isStatic)
{
	mIsStatic = isStatic;
}

RigidBody::RigidBody(float mass, bool isStatic)
{
	mMass = mass;
	mIsStatic = isStatic;
}

RigidBody::~RigidBody()
{
	if (mActor)
	{
		EntityManager::get()->unregisterRigidBody(this);	
		releaseShapes();
		mActor->release();
		mActor = nullptr;
	}
}

Vector3 RigidBody::getVelocity()
{
	if (!mActor) return Vector3();
	if (mIsStatic) return Vector3();

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxVec3 velocity = dynamicActor->getLinearVelocity();
	return Vector3(velocity.x, velocity.y, velocity.z);
}

Vector3 RigidBody::getVelocityAtPoint(const Vector3& point)
{
	if (!mActor) return Vector3();
	if (mIsStatic) return Vector3();

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxVec3 res = PxRigidBodyExt::getVelocityAtPos(*dynamicActor, PxVec3(point.x, point.y, point.z));

	return Vector3(res.x, res.y, res.z);
}

Vector3 RigidBody::getAngularVelocity()
{
	if (!mActor) return Vector3();
	if (mIsStatic) return Vector3();

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxVec3 angularVelocity = dynamicActor->getAngularVelocity();
	return Vector3(angularVelocity.x, angularVelocity.y, angularVelocity.z);
}

void RigidBody::addForce(const Vector3& force)
{
	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->addForce(PxVec3(force.x, force.y, force.z), PxForceMode::eFORCE, true);
}

void RigidBody::addForce(const Vector3& force, const Vector3& position)
{
	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxRigidBodyExt::addForceAtPos(*dynamicActor, PxVec3(force.x, force.y, force.z), PxVec3(position.x, position.y, position.z), PxForceMode::eFORCE, true);
}

void RigidBody::addTorque(const Vector3& torque)
{
	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->addTorque(PxVec3(torque.x, torque.y, torque.z), PxForceMode::eFORCE, true);
}

void RigidBody::setContinousCollisionDetection(bool ccd)
{
	mCcd = ccd;

	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, ccd);
}

void RigidBody::awake()
{
    Transform* t = mOwner->getTransform();
    Vector3 pos = t->getPosition();
    Vector3 euler = t->getRotation();
	mScale = t->getScale();

    PxQuat q = PxQuat(euler.x, PxVec3(1, 0, 0))
        * PxQuat(euler.y, PxVec3(0, 1, 0))
        * PxQuat(euler.z, PxVec3(0, 0, 1));
    PxTransform pose(PxVec3(pos.x, pos.y, pos.z), q);

    PhysicsEngine* physics = PhysicsEngine::get();

	if (mIsStatic)
	{
		mActor = physics->getPhysics()->createRigidStatic(pose);
	}
	else
	{
		mActor = physics->getPhysics()->createRigidDynamic(pose);
	}

	mMaterial = physics->getMaterial();

	std::list<Collider*> colliders = mOwner->getComponents<Collider>();

	for (Collider* c : colliders) {
		mColliders.push_back(c);
	}

	updateShape();

	if (!mIsStatic)
	{
		PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
		PxRigidBodyExt::updateMassAndInertia(*dynamicActor, mMass);
		dynamicActor->setMass(mMass);
		dynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, mCcd);
	}

    physics->getScene()->addActor(*mActor);
	EntityManager::get()->registerRigidBody(this);
}

void RigidBody::update()
{
}

void RigidBody::fixedUpdate()
{
	if (!mActor) return;

	PxTransform p = mActor->getGlobalPose();

	Transform* t = getOwner()->getTransform();
	t->setPosition({ p.p.x, p.p.y, p.p.z }, false);

	Quaternion quat{ p.q.x, p.q.y, p.q.z, p.q.w };
	Vector3 euler = quat.toEuler();
	t->setRotation(euler, false);

	if (!(mOwner->getTransform()->getScale() == mScale))
	{
		mScale = mOwner->getTransform()->getScale();

		updateShape();
	}
}

void RigidBody::updateShape()
{
	releaseShapes();

	for (Collider* c : mColliders) {
		PxShape* s = PhysicsEngine::get()->getPhysics()->createShape(*static_cast<PxGeometry*>(c->getGeometry(mScale, !mIsStatic)), *mMaterial);
		mActor->attachShape(*s);
	}
}

void RigidBody::releaseShapes()
{
	PxU32 shapesN = mActor->getNbShapes();
	std::vector<PxShape*> shapes(shapesN);

	mActor->getShapes(shapes.data(), shapesN);

	for (PxShape* shape : shapes) {
		mActor->detachShape(*shape);
		shape->release();
	}
}

void RigidBody::updateGlobalPose()
{
	if (!mActor) return;

	Transform* t = mOwner->getTransform();
	Vector3 pos = t->getPosition();
	Vector3 euler = t->getRotation();
	mScale = t->getScale();

	PxQuat q = PxQuat(euler.x, PxVec3(1, 0, 0))
		* PxQuat(euler.y, PxVec3(0, 1, 0))
		* PxQuat(euler.z, PxVec3(0, 0, 1));
	PxTransform pose(PxVec3(pos.x, pos.y, pos.z), q);

	mActor->setGlobalPose(pose, true);
}

void RigidBody::addCollider(Collider* collider)
{
	mColliders.push_back(collider);
}
