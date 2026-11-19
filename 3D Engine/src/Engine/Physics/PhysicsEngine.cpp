#include "PhysicsEngine.h"
#include <ctype.h>
#include <cmath>
#include "ConvexMeshManager.h"
#include "EntityManager.h"
#include "RigidBody.h"
#include <stdexcept>
#include <thread>
#include <iostream>
#include <string>

using namespace physx;

// Фільтр пар симуляції: тіла однієї ненульової групи не зіштовхуються, а для тіл з обліком зіткнень просить звіт про контакти
static PxFilterFlags engineFilterShader(PxFilterObjectAttributes attributes0, PxFilterData filterData0, PxFilterObjectAttributes attributes1, PxFilterData filterData1, PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	// Агенти однієї навчальної групи проходять одне крізь одного, але зіштовхуються з рештою сцени
	if (filterData0.word0 != 0 && filterData0.word0 == filterData1.word0) return PxFilterFlag::eSUPPRESS;

	pairFlags = PxPairFlag::eCONTACT_DEFAULT;

	// Звіт про контакти коштує часу, тому його просять лише тіла, яким він потрібен
	if ((filterData0.word1 | filterData1.word1) & 1u)
	{
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_CONTACT_POINTS;
	}

	return PxFilterFlag::eDEFAULT;
}

// Отримує від PhysX звіти про контакти й передає тілам з обліком зіткнень удари об перешкоди
class ContactReporter : public PxSimulationEventCallback
{
public:
	// Розбирає пари контактів після кроку симуляції
	void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override
	{
		if (pairHeader.flags & (PxContactPairHeaderFlag::eREMOVED_ACTOR_0 | PxContactPairHeaderFlag::eREMOVED_ACTOR_1)) return;

		RigidBody* body0 = EntityManager::get()->getRigidBody(pairHeader.actors[0]);
		RigidBody* body1 = EntityManager::get()->getRigidBody(pairHeader.actors[1]);

		bool report0 = body0 && body0->mReportCollisions;
		bool report1 = body1 && body1->mReportCollisions;

		if (!report0 && !report1) return;

		PxContactPairPoint points[16];

		for (PxU32 i = 0; i < nbPairs; i++)
		{
			const PxContactPair& pair = pairs[i];

			if (!(pair.events & (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS))) continue;

			PxU32 count = pair.extractContacts(points, 16);

			// Перешкодою вважається лише бічний удар: опору знизу корпус відчуває й на звичайному горбі
			bool obstacle = false;

			for (PxU32 p = 0; p < count; p++)
			{
				if (std::fabs(points[p].normal.y) < OBSTACLE_NORMAL_Y)
				{
					obstacle = true;
					break;
				}
			}

			if (!obstacle) continue;

			if (report0) body0->onCollision();
			if (report1) body1->onCollision();

			return;
		}
	}

	// Розрив з'єднань рушій не обробляє
	void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {}
	// Пробудження тіл рушій не обробляє
	void onWake(PxActor** actors, PxU32 count) override {}
	// Засинання тіл рушій не обробляє
	void onSleep(PxActor** actors, PxU32 count) override {}
	// Тригери рушій не обробляє
	void onTrigger(PxTriggerPair* pairs, PxU32 count) override {}
	// Ранні звіти про рух тіл рушій не обробляє
	void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {}

private:
	// Найбільша вертикальна складова нормалі контакту, за якої він вважається ударом об перешкоду, а не опорою
	static constexpr float OBSTACLE_NORMAL_Y = 0.7f;
};

// Єдиний приймач звітів про контакти для сцени PhysX
static ContactReporter gContactReporter;

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
    gDispatcher = PxDefaultCpuDispatcherCreate(std::thread::hardware_concurrency());
    sceneDesc.cpuDispatcher = gDispatcher;
    sceneDesc.filterShader = engineFilterShader;
    sceneDesc.simulationEventCallback = &gContactReporter;
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

	// Зміна цих параметрів вимагає нового номера в cookingStamp, інакше кеш віддасть дані, приготовані по-старому
	// BVH34 будується на порядок швидше за типовий BVH33 і не поступається йому у швидкості запитів
    params.midphaseDesc.setToDefault(PxMeshMidPhase::eBVH34);

	// Таблиця відповідності трикутників потрібна лише для матеріалів на кожен трикутник, які рушій не використовує
    params.suppressTriangleMeshRemapTable = true;

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

// Повертає відбиток версії PhysX і параметрів готування, від яких залежать готові дані
const char* PhysicsEngine::cookingStamp()
{
	// Друга частина - номер набору параметрів готування з init; його треба змінити разом з ними
	static const std::string stamp = std::to_string(PX_PHYSICS_VERSION) + "-bvh34-noremap-1";

	return stamp.c_str();
}

// Створює (готує) опуклий меш з набору точок і повертає його; cooked отримує готові дані для кешу
PxConvexMesh* PhysicsEngine::cookConvexMesh(const std::vector<PxVec3>& points, std::vector<unsigned char>* cooked)
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

    if (cooked) cooked->assign(buf.getData(), buf.getData() + buf.getSize());

    PxDefaultMemoryInputData input(buf.getData(), buf.getSize());
    PxConvexMesh* convexMesh = gPhysics->createConvexMesh(input);

    return convexMesh;
}

// Створює опуклий меш з уже готових даних, або nullptr, якщо дані не підходять
PxConvexMesh* PhysicsEngine::createConvexMesh(const std::vector<unsigned char>& cooked)
{
    if (cooked.empty()) return nullptr;

    PxDefaultMemoryInputData input(const_cast<PxU8*>(cooked.data()), (PxU32)cooked.size());

    return gPhysics->createConvexMesh(input);
}

// Створює трикутний меш з уже готових даних, або nullptr, якщо дані не підходять
PxTriangleMesh* PhysicsEngine::createTriangleMesh(const std::vector<unsigned char>& cooked)
{
    if (cooked.empty()) return nullptr;

    PxDefaultMemoryInputData input(const_cast<PxU8*>(cooked.data()), (PxU32)cooked.size());

    return gPhysics->createTriangleMesh(input);
}

// Створює (готує) трикутний меш з набору точок та індексів і повертає його; cooked отримує готові дані для кешу
PxTriangleMesh* PhysicsEngine::cookTriangleMesh(const std::vector<PxVec3>& points, const std::vector<PxU32>& indices, std::vector<unsigned char>* cooked)
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

    if (cooked) cooked->assign(buf.getData(), buf.getData() + buf.getSize());

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
