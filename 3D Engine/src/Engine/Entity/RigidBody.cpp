#include "RigidBody.h"
#include "PhysicsEngine.h"
#include "ConvexMeshManager.h"
#include "Entity.h"
#include "Quaternion.h"
#include "Collider.h"
#include "EntityManager.h"
#include "Properties.h"

using namespace physx;

// Конструктор класу RigidBody для статичного або динамічного тіла
RigidBody::RigidBody(bool isStatic)
{
	mIsStatic = isStatic;
}

// Конструктор класу RigidBody з масою та типом (статичний/динамічний)
RigidBody::RigidBody(float mass, bool isStatic)
{
	mMass = mass;
	mIsStatic = isStatic;
}

// Деструктор класу RigidBody, звільняє ресурси PhysX та знімає реєстрацію в EntityManager
RigidBody::~RigidBody()
{
	if (mActor)
	{
		EntityManager::get()->unregisterRigidBody(this);	
		releaseShapes();
		mActor->release();
		mActor = nullptr;
	}
}

// Повертає лінійну швидкість тіла
Vector3 RigidBody::getVelocity()
{
	if (!mActor) return Vector3();
	if (mIsStatic) return Vector3();

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxVec3 velocity = dynamicActor->getLinearVelocity();
	return Vector3(velocity.x, velocity.y, velocity.z);
}

// Повертає швидкість тіла у заданій точці
Vector3 RigidBody::getVelocityAtPoint(const Vector3& point)
{
	if (!mActor) return Vector3();
	if (mIsStatic) return Vector3();

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxVec3 res = PxRigidBodyExt::getVelocityAtPos(*dynamicActor, PxVec3(point.x, point.y, point.z));

	return Vector3(res.x, res.y, res.z);
}

// Повертає кутову швидкість тіла
Vector3 RigidBody::getAngularVelocity()
{
	if (!mActor) return Vector3();
	if (mIsStatic) return Vector3();

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxVec3 angularVelocity = dynamicActor->getAngularVelocity();
	return Vector3(angularVelocity.x, angularVelocity.y, angularVelocity.z);
}

// Застосовує силу до тіла
void RigidBody::addForce(const Vector3& force)
{
	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->addForce(PxVec3(force.x, force.y, force.z), PxForceMode::eFORCE, true);
}

// Застосовує силу до тіла в певній точці
void RigidBody::addForce(const Vector3& force, const Vector3& position)
{
	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	PxRigidBodyExt::addForceAtPos(*dynamicActor, PxVec3(force.x, force.y, force.z), PxVec3(position.x, position.y, position.z), PxForceMode::eFORCE, true);
}

// Застосовує кутову силу до тіла
void RigidBody::addTorque(const Vector3& torque)
{
	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->addTorque(PxVec3(torque.x, torque.y, torque.z), PxForceMode::eFORCE, true);
}

// Вмикає або вимикає режим безперервного виявлення зіткнень (CCD)
void RigidBody::setContinousCollisionDetection(bool ccd)
{
	mCcd = ccd;

	if (!mActor) return;
	if (mIsStatic) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, ccd);
}

// Перевіряє, чи тіло нерухоме
bool RigidBody::isStatic() const
{
	return mIsStatic;
}

// Повертає масу тіла
float RigidBody::getMass() const
{
	return mMass;
}

// Робить тіло нерухомим або рухомим, замінивши актора PhysX новим
void RigidBody::setStatic(bool isStatic)
{
	if (mIsStatic == isStatic) return;

	// До awake актора ще немає, тому достатньо запам'ятати потрібний тип
	if (!mActor)
	{
		mIsStatic = isStatic;
		return;
	}

	// Поза береться з самого актора: вона свіжіша за Transform у момент перемикання
	PxTransform pose = mActor->getGlobalPose();

	releaseShapes();
	mActor->release();
	mActor = nullptr;

	mIsStatic = isStatic;

	PhysicsEngine* physics = PhysicsEngine::get();

	if (mIsStatic) mActor = physics->getPhysics()->createRigidStatic(pose);
	else mActor = physics->getPhysics()->createRigidDynamic(pose);

	// Форми будуються наново, бо рухоме тіло PhysX приймає лише з опуклою оболонкою,
	// а трикутну сітку дозволяє тільки нерухомому
	updateShape();

	if (!mIsStatic)
	{
		PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
		PxRigidBodyExt::updateMassAndInertia(*dynamicActor, mMass);
		dynamicActor->setMass(mMass);
		dynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, mCcd);
	}

	physics->getScene()->addActor(*mActor);
}
void RigidBody::setMass(float mass)
{
	if (mIsStatic) return;

	mMass = mass;

	if (!mActor) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->setMass(mass);
	PxRigidBodyExt::updateMassAndInertia(*dynamicActor, mass);
}

void RigidBody::setCenterOfMass(const Vector3& com)
{
	if (mIsStatic) return;
	if (!mActor) return;

	PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
	dynamicActor->setCMassLocalPose(PxTransform(PxVec3(com.x, com.y, com.z)));
}

// Ініціалізує фізичне тіло, створює фізичного актора, додає коллайдери, реєструє в EntityManager
void RigidBody::awake()
{
	// Беремо Transform з власника
    Transform* t = mOwner->getTransform();
    Vector3 pos = t->getPosition();
    Vector3 euler = t->getRotation();
	mScale = t->getScale();

    PxQuat q = PxQuat(euler.x, PxVec3(1, 0, 0))
        * PxQuat(euler.y, PxVec3(0, 1, 0))
        * PxQuat(euler.z, PxVec3(0, 0, 1));
    PxTransform pose(PxVec3(pos.x, pos.y, pos.z), q);

    PhysicsEngine* physics = PhysicsEngine::get();

	// Створюємо фізичного актора в залежності від типу (статичний чи динамічний)
	if (mIsStatic)
	{
		mActor = physics->getPhysics()->createRigidStatic(pose);
	}
	else
	{
		mActor = physics->getPhysics()->createRigidDynamic(pose);
	}

	mMaterial = physics->getMaterial();

	// Беремо всі коллайдери з власника
	std::list<Collider*> colliders = mOwner->getComponents<Collider>();

	// Додаємо коллайдери до фізичного тіла
	for (Collider* c : colliders) {
		mColliders.push_back(c);
	}

	updateShape();

	// Якщо це динамічне тіло, встановлюємо масу та інерцію
	if (!mIsStatic)
	{
		PxRigidDynamic* dynamicActor = static_cast<PxRigidDynamic*>(mActor);
		PxRigidBodyExt::updateMassAndInertia(*dynamicActor, mMass);
		dynamicActor->setMass(mMass);
		dynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, mCcd);
	}

	// Реєструємо фізичне тіло в PhysX сцені та в EntityManager
    physics->getScene()->addActor(*mActor);
	EntityManager::get()->registerRigidBody(this);
}

void RigidBody::update()
{
}

// Фіксоване оновлення компонента: синхронізує позицію та обертання з PhysX
void RigidBody::fixedUpdate()
{
	if (!mActor) return;

	PxTransform p = mActor->getGlobalPose();

	Transform* t = getOwner()->getTransform();
	t->setPosition({ p.p.x, p.p.y, p.p.z }, false);

	Quaternion quat{ p.q.x, p.q.y, p.q.z, p.q.w };
	Vector3 euler = quat.toEuler();
	t->setRotation(euler, false);
}

// Оновлює коллайдери фізичного тіла відповідно до поточного масштабу
void RigidBody::updateShape()
{
	releaseShapes();

	for (Collider* c : mColliders) {
		void* geometry = c->getGeometry(mScale, !mIsStatic);

		// Коллайдер без меша не має що дати фізиці, а розіменування nullptr тут впало б
		if (geometry == nullptr) continue;

		PxShape* s = PhysicsEngine::get()->getPhysics()->createShape(*static_cast<PxGeometry*>(geometry), *mMaterial);
		mActor->attachShape(*s);
	}
}

// Звільняє всі форми з фізичного тіла
void RigidBody::releaseShapes()
{
	PxU32 shapesN = mActor->getNbShapes();
	std::vector<PxShape*> shapes(shapesN);

	mActor->getShapes(shapes.data(), shapesN);

	for (PxShape* shape : shapes) {
		mActor->detachShape(*shape);
		shape->release();
	}
}

// Оновлює глобальну позицію та обертання фізичного тіла згідно з Transform
void RigidBody::updateGlobalPose()
{
	if (!mActor) return;

	Transform* t = mOwner->getTransform();
	Vector3 pos = t->getPosition();
	Vector3 euler = t->getRotation();

	if (!(mScale == t->getScale()))
	{
		mScale = t->getScale();
		updateShape();
	}

	PxQuat q = PxQuat(euler.x, PxVec3(1, 0, 0))
		* PxQuat(euler.y, PxVec3(0, 1, 0))
		* PxQuat(euler.z, PxVec3(0, 0, 1));
	PxTransform pose(PxVec3(pos.x, pos.y, pos.z), q);

	mActor->setGlobalPose(pose, true);
}

// Забуває прибраний коллайдер і перебудовує форми без нього
void RigidBody::onComponentRemoved(Component* removed)
{
	Collider* collider = dynamic_cast<Collider*>(removed);

	if (collider == nullptr) return;

	size_t before = mColliders.size();

	mColliders.remove(collider);

	if (mColliders.size() == before) return;

	// Форма коллайдера досі прикріплена до актора і далі зіштовхувалася б з усім довкола
	if (mActor) updateShape();
}

// Додає коллайдер до списку коллайдерів цього фізичного тіла
void RigidBody::addCollider(Collider* collider)
{
	mColliders.push_back(collider);
}
// Перелічує власні поля для файлу сцени та інспектора
void RigidBody::visitProperties(PropertyVisitor& visitor)
{
	bool isStatic = mIsStatic;
	visitor.property("static", isStatic);

	// Тип тіла міняє самого актора PhysX, тому його не можна просто привласнити полю
	if (isStatic != mIsStatic) setStatic(isStatic);

	float mass = mMass;

	// Нерухоме тіло маси не має, тому показуємо її, але не даємо правити
	if (mIsStatic) visitor.beginReadOnly();

	visitor.property("mass", mass, 0.1f);

	if (mIsStatic) visitor.endReadOnly();

	// Масу мало записати у поле: її має знати й сам рушій фізики
	if (mass != mMass) setMass(mass);
}
