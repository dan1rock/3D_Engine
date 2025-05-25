#include "PhysicsEngine.h"
#include <ctype.h>
#include "ConvexMeshManager.h"
#include <stdexcept>

using namespace physx;

PhysicsEngine::PhysicsEngine()
{
	mConvexMeshManager = new ConvexMeshManager();
}

PhysicsEngine::~PhysicsEngine()
{
    delete mConvexMeshManager;
}

// Ініціалізує PhysX, створює сцену, матеріали, диспатчер та інші ресурси
void PhysicsEngine::init()
{
	// Створення аллокатора та обробника помилок
    gFoundation = PxCreateFoundation(
        PX_PHYSICS_VERSION,
        gAllocator,
        gErrorCallback
    );

	// Створення PVD (Physics Visual Debugger) для відладки
    gPvd = PxCreatePvd(*gFoundation);
    PxPvdTransport* transport =
        PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
    gPvd->connect(
        *transport,
        PxPvdInstrumentationFlag::eALL
    );

	// Створення інстансу PhysX Physics API
    gPhysics = PxCreatePhysics(
        PX_PHYSICS_VERSION,
        *gFoundation,
        PxTolerancesScale(),
        true,
        gPvd
    );

	// Створення матеріалу за замовчуванням
    gMaterial = gPhysics->createMaterial(0.5f, 0.5f, 0.6f);

	// Створення сцени
    PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
    sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	sceneDesc.flags |= PxSceneFlag::eENABLE_CCD;
    sceneDesc.ccdThreshold = 0.001f;
    sceneDesc.ccdMaxPasses = 4;
    gDispatcher = PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = gDispatcher;
    sceneDesc.filterShader = PxDefaultSimulationFilterShader;
    gScene = gPhysics->createScene(sceneDesc);

	// Ініціалізація PVD (Physics Visual Debugger) для сцени
    if (auto* client = gScene->getScenePvdClient())
    {
        client->setScenePvdFlag(
            PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS, true);
        client->setScenePvdFlag(
            PxPvdSceneFlag::eTRANSMIT_CONTACTS, true);
        client->setScenePvdFlag(
            PxPvdSceneFlag::eTRANSMIT_SCENEQUERIES, true);
    }

	// Створення інтерфейсу для роботи з мешами
    PxCookingParams params(gPhysics->getTolerancesScale());
    gCooking = PxCreateCooking(PX_PHYSICS_VERSION, *gFoundation, params);
}

// Оновлює фізичну сцену (симуляція) на заданий крок часу
void PhysicsEngine::update(float deltaTime)
{
	if (deltaTime <= 0.0f)
		return;

	gScene->simulate(deltaTime);
	gScene->fetchResults(true);
}

// Завершує роботу PhysX, звільняє всі ресурси
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

// Повертає вказівник на об'єкт PxPhysics
PxPhysics* PhysicsEngine::getPhysics()
{
    return gPhysics;
}

// Повертає вказівник на об'єкт PxScene
PxScene* PhysicsEngine::getScene()
{
    return gScene;
}

// Повертає вказівник на фізичний матеріал за замовчуванням
PxMaterial* PhysicsEngine::getMaterial()
{
    return gMaterial;
}

// Повертає менеджер опуклих мешів
ConvexMeshManager* PhysicsEngine::getConvexMeshManager()
{
    return mConvexMeshManager;
}

// Створює (готує) опуклий меш з набору точок і повертає його
PxConvexMesh* PhysicsEngine::cookConvexMesh(const std::vector<PxVec3>& points)
{
    PxConvexMeshDesc desc;
    desc.points.count = static_cast<PxU32>(points.size());
    desc.points.stride = sizeof(PxVec3);
    desc.points.data = points.data();
    desc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

    PxDefaultMemoryOutputStream buf;
    if (!gCooking->cookConvexMesh(desc, buf)) {
        throw std::runtime_error("Convex cooking failed");
    }

    PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
    PxConvexMesh* convexMesh = gPhysics->createConvexMesh(input);

    return convexMesh;
}

// Створює (готує) трикутний меш з набору точок та індексів і повертає його
PxTriangleMesh* PhysicsEngine::cookTriangleMesh(const std::vector<PxVec3>& points, const std::vector<PxU32>& indices)
{
    PxTriangleMeshDesc meshDesc;
    meshDesc.points.count = PxU32(points.size());
    meshDesc.points.stride = sizeof(PxVec3);
    meshDesc.points.data = points.data();
    meshDesc.triangles.count = PxU32(indices.size() / 3);
    meshDesc.triangles.stride = 3 * sizeof(uint32_t);
    meshDesc.triangles.data = indices.data();

    PxDefaultMemoryOutputStream buf;
    if (!gCooking->cookTriangleMesh(meshDesc, buf))
        throw std::runtime_error("Failed to cook triangle mesh");

    PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
    PxTriangleMesh* triangleMesh = gPhysics->createTriangleMesh(input);

    return triangleMesh;
}

// Повертає єдиний екземпляр PhysicsEngine (синглтон)
PhysicsEngine* PhysicsEngine::get()
{
	static PhysicsEngine instance;
	return &instance;
}
