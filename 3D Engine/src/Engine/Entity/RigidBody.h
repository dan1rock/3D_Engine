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
	COMPONENT_TYPE(RigidBody)

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
	// Робить тіло нерухомим або рухомим. PhysX не перетворює актора на місці, тому старого
	// замінює новий: поза та коллайдери переносяться, набута швидкість втрачається
	void setStatic(bool isStatic);
	// Встановлює масу тіла
	void setMass(float mass);
	// Встановлює центр мас тіла
	void setCenterOfMass(const Vector3& com);
	// Задає лінійну швидкість тіла
	void setVelocity(const Vector3& velocity);
	// Задає кутову швидкість тіла
	void setAngularVelocity(const Vector3& velocity);

	// Задає групу фільтрації: тіла з однаковою ненульовою групою не зіштовхуються й не бачать одне одного в запитах
	void setCollisionGroup(unsigned int group);
	// Повертає групу фільтрації зіткнень, 0 - тіло без групи
	unsigned int getCollisionGroup() const;
	// Вмикає облік зіткнень тіла з перешкодами, який забирає takeCollisions
	void setReportCollisions(bool report);
	// Повертає кількість зіткнень з перешкодами від попереднього виклику й обнуляє лічильник
	int takeCollisions();

	// Додає актора до сцени PhysX або прибирає з неї, щоб неактивний об'єкт не брав участі у фізиці
	void syncActivity();

	// Перевіряє, чи тіло нерухоме
	bool isStatic() const;
	// Повертає масу тіла
	float getMass() const;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

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

	// Забуває прибраний коллайдер і перебудовує форми без нього
	void onComponentRemoved(Component* removed) override;

	// Додає коллайдер до списку коллайдерів цього фізичного тіла
	void addCollider(Collider* collider);

	// Передає формам актора групу фільтрації та ознаку обліку зіткнень
	void applyFilterData();
	// Викликається рушієм фізики, коли тіло вдарилося об перешкоду
	void onCollision();

	std::list<Collider*> mColliders = {};

    physx::PxRigidActor* mActor = nullptr;
	physx::PxMaterial* mMaterial = nullptr;

	Vector3 mScale;

	float mMass = 1.0f;
	bool mIsStatic = false;
	bool mCcd = false;
	// Чи актор зараз у сцені PhysX; неактивний об'єкт тримає актора поза нею
	bool mInScene = false;
	// Група фільтрації зіткнень, 0 - без групи
	unsigned int mCollisionGroup = 0;
	// Чи рахувати зіткнення тіла з перешкодами
	bool mReportCollisions = false;
	// Кількість зіткнень від останнього takeCollisions
	int mCollisions = 0;

	friend class Transform;
	friend class Collider;
	friend class EntityManager;
	friend class Physics;
	friend class ContactReporter;
};

