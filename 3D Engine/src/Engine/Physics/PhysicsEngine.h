#pragma once
#include <PxPhysicsAPI.h>
#include <vector>

using namespace physx;

class ConvexMeshManager;

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

	ConvexMeshManager* getConvexMeshManager();

	PxConvexMesh* cookConvexMesh(const std::vector<PxVec3>& points);
	PxTriangleMesh* cookTriangleMesh(const std::vector<PxVec3>& points, const std::vector<PxU32>& indices);

	static PhysicsEngine* get();

private:
	ConvexMeshManager* mConvexMeshManager = nullptr;

	PxDefaultAllocator gAllocator;
	PxDefaultErrorCallback gErrorCallback;
	PxFoundation* gFoundation = NULL;
	PxPhysics* gPhysics = NULL;
	PxDefaultCpuDispatcher* gDispatcher = NULL;
	PxScene* gScene = NULL;
	PxPvd* gPvd = NULL;

	PxMaterial* gMaterial = NULL;

	PxCooking* gCooking = NULL;
};

