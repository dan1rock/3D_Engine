#pragma once
#include "Component.h"
#include <PxPhysicsAPI.h>
#include "Vector3.h"
#include <list>

class Collider;

// Компонент, що представляє фізичний об'єкт у сцені
class RigidBody : public Component
{
public:
	// Конструктор класу RigidBody для статичного або динамічного тіла
	RigidBody(bool isStatic = false);
	// Конструктор класу RigidBody з масою та типом (статичний/динамічний)
    RigidBody(float mass, bool isStatic = false);
	// Деструктор класу RigidBody, звільняє ресурси PhysX та знімає реєстрацію в EntityManager
    ~RigidBody() override;

	// Повертає лінійну швидкість тіла
	Vector3 getVelocity();
	// Повертає швидкість тіла у заданій точці
	Vector3 getVelocityAtPoint(const Vector3& point);
	// Повертає кутову швидкість тіла
	Vector3 getAngularVelocity();

	// Додає силу до тіла
	void addForce(const Vector3& force);
	// Додає силу до тіла в певній точці
	void addForce(const Vector3& force, const Vector3& position);
	// Додає кутову силу до тіла
	void addTorque(const Vector3& torque);
	// Вмикає або вимикає режим безперервного виявлення зіткнень (CCD)
	void setContinousCollisionDetection(bool ccd);
	// Встановлює масу тіла
	void setMass(float mass);
	// Встановлює центр мас тіла
	void setCenterOfMass(const Vector3& com);

protected:
	// Конструктор копіювання
	RigidBody* instantiate() const override {
		return new RigidBody(*this);
	};

private:
	// Ініціалізує фізичне тіло, створює фізичного актора, додає коллайдери, реєструє в EntityManager
	void awake() override;
	void update() override;
	// Фіксоване оновлення компонента: синхронізує позицію та обертання з PhysX
	void fixedUpdate() override;

	// Оновлює коллайдери фізичного тіла відповідно до поточного масштабу
	void updateShape();
	// Звільняє всі форми з фізичного тіла
	void releaseShapes();

	// Оновлює глобальну позицію та обертання фізичного тіла згідно з Transform
	void updateGlobalPose();

	// Додає коллайдер до списку коллайдерів цього фізичного тіла
	void addCollider(Collider* collider);

	std::list<Collider*> mColliders = {};

    physx::PxRigidActor* mActor = nullptr;
	physx::PxMaterial* mMaterial = nullptr;

	Vector3 mScale;

	float mMass = 1.0f;
	bool mIsStatic = false;
	bool mCcd = false;

	friend class Transform;
	friend class Collider;
	friend class EntityManager;
	friend class Physics;
};

