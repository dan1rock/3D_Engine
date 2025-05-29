#pragma once
#include <PxPhysicsAPI.h>
#include <vector>

using namespace physx;

class ConvexMeshManager;

class PhysicsEngine
{
public:
	// Ініціалізує PhysX, створює сцену, матеріали, диспатчер та інші ресурси
	void init();
	// Оновлює фізичну сцену (симуляція) на заданий крок часу
	void update(float deltaTime);
	// Завершує роботу PhysX, звільняє всі ресурси
	void shutdown();

	// Повертає вказівник на об'єкт PxPhysics
	PxPhysics* getPhysics();
	// Повертає вказівник на об'єкт PxScene
	PxScene* getScene();
	// Повертає вказівник на фізичний матеріал за замовчуванням
	PxMaterial* getMaterial();

	// Повертає менеджер опуклих мешів
	ConvexMeshManager* getConvexMeshManager();

	// Створює (готує) опуклий меш з набору точок і повертає його
	PxConvexMesh* cookConvexMesh(const std::vector<PxVec3>& points);
	// Створює (готує) трикутний меш з набору точок та індексів і повертає його
	PxTriangleMesh* cookTriangleMesh(const std::vector<PxVec3>& points, const std::vector<PxU32>& indices);

	// Повертає єдиний екземпляр PhysicsEngine (синглтон)
	static PhysicsEngine* get();

private:
	PhysicsEngine();
	~PhysicsEngine();

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

