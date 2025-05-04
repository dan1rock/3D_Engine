#include "PhysicsEngine.h"
#include <ctype.h>

using namespace physx;

PhysicsEngine::PhysicsEngine()
{
}

PhysicsEngine::~PhysicsEngine()
{
}

void PhysicsEngine::init()
{
    gFoundation = PxCreateFoundation(
        PX_PHYSICS_VERSION,
        gAllocator,
        gErrorCallback
    );

    gPvd = PxCreatePvd(*gFoundation);
    PxPvdTransport* transport =
        PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    gPvd->connect(
        *transport,
        PxPvdInstrumentationFlag::eALL
    );

    gPhysics = PxCreatePhysics(
        PX_PHYSICS_VERSION,
        *gFoundation,
        PxTolerancesScale(),
        true,
        gPvd
    );

    gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.6f);

    PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
    gDispatcher = PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = gDispatcher;
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;
    gScene = gPhysics->createScene(sceneDesc);

    if (auto* client = gScene->getScenePvdClient())
    {
        client->setScenePvdFlag(
            PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        client->setScenePvdFlag(
            PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        client->setScenePvdFlag(
            PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }
}

void PhysicsEngine::update(float deltaTime)
{
	if (deltaTime <= 0.0f)
		return;

	gScene->simulate(deltaTime);
	gScene->fetchResults(true);
}

void PhysicsEngine::shutdown()
{
    if (gScene) {
        gScene->release();
        gScene = nullptr;
    }
    if (gDispatcher) {
        gDispatcher->release();
        gDispatcher = nullptr;
    }
    if (gPhysics) {
        gPhysics->release();
        gPhysics = nullptr;
    }

    if (gPvd) {
        auto* transport = gPvd->getTransport();
        gPvd->release();
        gPvd = nullptr;
        if (transport) {
            transport->release();
            transport = nullptr;
        }
    }

    if (gFoundation) {
        gFoundation->release();
        gFoundation = nullptr;
    }
}

PxPhysics* PhysicsEngine::getPhysics()
{
    return gPhysics;
}

PxScene* PhysicsEngine::getScene()
{
    return gScene;
}

PxMaterial* PhysicsEngine::getMaterial()
{
    return gMaterial;
}

PhysicsEngine* PhysicsEngine::get()
{
	static PhysicsEngine instance;
	return &instance;
}
