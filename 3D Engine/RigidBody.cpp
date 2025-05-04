#include "RigidBody.h"
#include "PhysicsEngine.h"
#include "GameObject.h"
#include "Quaternion.h"

using namespace physx;

RigidBody::RigidBody()
{
}

RigidBody::RigidBody(float radius, float mass)
{
	mRadius = radius;
	mMass = mass;
}

RigidBody::~RigidBody()
{
	if (mActor)
	{
		mActor->release();
		mActor = nullptr;
	}
}

void RigidBody::awake()
{
    Transform* t = mOwner->getTransform();
    Vector3 pos = t->getPosition();
    Vector3 euler = t->getRotation();

    PxQuat q = PxQuat(euler.x, PxVec3(1, 0, 0))
        * PxQuat(euler.y, PxVec3(0, 1, 0))
        * PxQuat(euler.z, PxVec3(0, 0, 1));
    PxTransform pose(PxVec3(pos.x, pos.y, pos.z), q);

    PhysicsEngine* physics = PhysicsEngine::get();
    mActor = physics->getPhysics()->createRigidDynamic(pose);

    PxMaterial* mat = physics->getMaterial();
    PxShape* shape = physics->getPhysics()->createShape(PxSphereGeometry(mRadius), *mat);
    mActor->attachShape(*shape);

    PxRigidBodyExt::updateMassAndInertia(*mActor, mMass);

    physics->getScene()->addActor(*mActor);
}

void RigidBody::update()
{
    if (!mActor) return;

    PxTransform p = mActor->getGlobalPose();

    Transform* t = getOwner()->getTransform();
    t->setPosition({ p.p.x, p.p.y, p.p.z });

    Quaternion quat{ p.q.x, p.q.y, p.q.z, p.q.w };
    Vector3 euler = quat.toEuler();
    t->setRotation(euler);
}
