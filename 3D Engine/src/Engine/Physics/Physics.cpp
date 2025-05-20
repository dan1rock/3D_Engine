#include "Physics.h"
#include "PhysicsEngine.h"
#include "EntityManager.h"
#include "RigidBody.h"

using namespace physx;

struct IgnoreActorFilterCallback : PxQueryFilterCallback
{
    PxRigidActor* mIgnore;

    IgnoreActorFilterCallback(PxRigidActor* ignore) : mIgnore(ignore) {}

    // Called before the query hits get added
    PxQueryHitType::Enum preFilter(
        PxFilterData const& filterData,
        const PxShape* shape,
        const PxRigidActor* actor,
        PxHitFlags& queryFlags
    ) override
    {
        if (actor == mIgnore)
            return PxQueryHitType::eNONE;

        return PxQueryHitType::eBLOCK;
    }

    PxQueryHitType::Enum postFilter(
        PxFilterData const& filterData,
        const PxQueryHit& hit
    ) override
    {
        return PxQueryHitType::eNONE;
    }
};

bool Physics::raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, RigidBody* ignore)
{
    PxRaycastBuffer hitBuffer;

    PxQueryFilterData filterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);
    filterData.flags |= PxQueryFlag::ePREFILTER;
    IgnoreActorFilterCallback filterCB(ignore->mActor);

    bool status = PhysicsEngine::get()->getScene()->raycast(
        PxVec3(origin.x, origin.y, origin.z),
        PxVec3(direction.x, direction.y, direction.z).getNormalized(),
        maxDistance,
        hitBuffer,
        PxHitFlag::eDEFAULT,
        filterData,
        &filterCB
    );

	if (!status || !hitBuffer.hasBlock)
		return false;

    outHit.distance = hitBuffer.block.distance;
    outHit.point = Vector3(hitBuffer.block.position.x, hitBuffer.block.position.y, hitBuffer.block.position.z);
    outHit.normal = Vector3(hitBuffer.block.normal.x, hitBuffer.block.normal.y, hitBuffer.block.normal.z);
	outHit.rigidBody = EntityManager::get()->getRigidBody(hitBuffer.block.actor);

    return true;
}

bool Physics::sphereCast(const Vector3& origin, const Vector3& direction, float radius, float maxDistance, RaycastHit& outHit, RigidBody* ignore)
{
	PxSphereGeometry sphere(radius);

    PxTransform startPose(PxVec3(origin.x, origin.y, origin.z), PxQuat(PxIdentity));

    PxSweepBuffer sweepBuffer;

    PxQueryFilterData filterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);

	bool status = PhysicsEngine::get()->getScene()->sweep(
		sphere,
		startPose,
		PxVec3(direction.x, direction.y, direction.z).getNormalized(),
		maxDistance,
		sweepBuffer,
		PxHitFlag::eDEFAULT,
		filterData
	);

    if (!status || !sweepBuffer.hasBlock)
        return false;

	outHit.distance = sweepBuffer.block.distance;
	outHit.point = Vector3(sweepBuffer.block.position.x, sweepBuffer.block.position.y, sweepBuffer.block.position.z);
	outHit.normal = Vector3(sweepBuffer.block.normal.x, sweepBuffer.block.normal.y, sweepBuffer.block.normal.z);
	outHit.rigidBody = EntityManager::get()->getRigidBody(sweepBuffer.block.actor);

	return true;
}

bool Physics::overlapSphere(const Vector3& origin, float radius, std::vector<OverlapHit>& outHits)
{
    PxSphereGeometry sphere(radius);
    PxTransform pose(PxVec3(origin.x, origin.y, origin.z));

    PxOverlapBuffer overlapBuffer;

    PxQueryFilterData filterData(PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC);

    bool status = PhysicsEngine::get()->getScene()->overlap(
        sphere,
        pose,
        overlapBuffer,
        filterData
    );

    if (!status || overlapBuffer.nbTouches == 0)
        return false;

    outHits.clear();
    outHits.reserve(overlapBuffer.nbTouches);
    for (PxU32 i = 0; i < overlapBuffer.nbTouches; ++i)
    {
        outHits.push_back({
            EntityManager::get()->getRigidBody(overlapBuffer.touches[i].actor),
            nullptr
        });
    }

    return true;
}
