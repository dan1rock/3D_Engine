#include "RigidBody.h"
#include "PhysicsEngine.h"
#include "ConvexMeshManager.h"
#include "GameObject.h"
#include "Quaternion.h"
#include "MeshRenderer.h"

using namespace physx;

RigidBody::RigidBody(bool isStatic)
{
	mIsStatic = isStatic;
}

RigidBody::RigidBody(float radius, float mass, bool isStatic)
{
	mRadius = radius;
	mMass = mass;
	mIsStatic = isStatic;
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

	if (mIsStatic)
	{
		mActor = physics->getPhysics()->createRigidStatic(pose);
	}
	else
	{
		mActor = physics->getPhysics()->createRigidDynamic(pose);
	}

	PxGeometry* geometry = nullptr;

	MeshRenderer* meshRenderer = mOwner->getComponent<MeshRenderer>();
	if (meshRenderer) {
		ConvexMesh* convexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(meshRenderer->getMesh()->getFullPath().c_str());
		geometry = new PxConvexMeshGeometry(static_cast<PxConvexMesh*>(convexMesh->getConvexMesh()));
	}
	else {
		geometry = new PxSphereGeometry(mRadius);
	}

    PxMaterial* mat = physics->getMaterial();
    PxShape* shape = physics->getPhysics()->createShape(*geometry, *mat);
    mActor->attachShape(*shape);

	if (!mIsStatic)
	{
		PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
		dynamicActor->setMass(mMass);
		dynamicActor->setMassSpaceInertiaTensor(PxVec3(1, 1, 1));
	}

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
