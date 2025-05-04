#pragma once
#include <PxPhysicsAPI.h>

using namespace physx;

class PhysicsEngine
{
public:
	PhysicsEngine();
	~PhysicsEngine();

	void init();
	void update(float deltaTime);
	void shutdown();

	PxPhysics* getPhysics();
	PxScene* getScene();
	PxMaterial* getMaterial();

	static PhysicsEngine* get();

private:
	PxDefaultAllocator gAllocator;
	PxDefaultErrorCallback gErrorCallback;
	PxFoundation* gFoundation = NULL;
	PxPhysics* gPhysics = NULL;
	PxDefaultCpuDispatcher* gDispatcher = NULL;
	PxScene* gScene = NULL;
	PxPvd* gPvd = NULL;

	PxMaterial* gMaterial = NULL;
};

