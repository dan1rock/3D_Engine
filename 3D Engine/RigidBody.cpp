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

	updateShape();

    mActor->attachShape(*mShape);

	if (!mIsStatic)
	{
		PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
		PxRigidBodyExt::updateMassAndInertia(*dynamicActor, mMass);
	}

    physics->getScene()->addActor(*mActor);
}

void RigidBody::update()
{
}

void RigidBody::fixedUpdate()
{
	if (!mActor) return;

	PxTransform p = mActor->getGlobalPose();

	Transform* t = getOwner()->getTransform();
	t->setPosition({ p.p.x, p.p.y, p.p.z });

	Quaternion quat{ p.q.x, p.q.y, p.q.z, p.q.w };
	Vector3 euler = quat.toEuler();
	t->setRotation(euler);

	if (!(mOwner->getTransform()->getScale() == mScale))
	{
		mScale = mOwner->getTransform()->getScale();

		mActor->detachShape(*mShape);
		updateShape();
		mActor->attachShape(*mShape);
	}
}

void RigidBody::updateShape()
{
	PxGeometry* geometry = nullptr;
	PxMeshScale meshScale(PxVec3(mScale.x, mScale.y, mScale.z), PxQuat(PxIdentity));

	MeshRenderer* meshRenderer = mOwner->getComponent<MeshRenderer>();
	if (meshRenderer) {
		ConvexMesh* convexMesh = PhysicsEngine::get()->getConvexMeshManager()->createConvexMeshFromFile(meshRenderer->getMesh()->getFullPath().c_str());

		if (mIsStatic)
		{
			geometry = new PxTriangleMeshGeometry(static_cast<PxTriangleMesh*>(convexMesh->getTriangleMesh()), meshScale);
		}
		else
		{
			geometry = new PxConvexMeshGeometry(static_cast<PxConvexMesh*>(convexMesh->getConvexMesh()), meshScale);
		}
	}
	else {
		geometry = new PxSphereGeometry(mRadius);
	}

	if (mShape)
	{
		mShape->release();
		mShape = nullptr;
	}

	mShape = PhysicsEngine::get()->getPhysics()->createShape(*geometry, *mMaterial);
}
