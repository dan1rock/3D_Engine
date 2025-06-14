#pragma once
#include "Transform.h"
#include <list>

class RigidBody;

// Представляє об'єкт у сцені, що може мати компоненти
class Entity
{
public:
	// Конструктор класу GameObject, реєструє об'єкт у EntityManager та ініціалізує трансформацію за замовчуванням
	Entity();
	// Конструктор класу GameObject з початковою позицією
	Entity(Vector3 position);
	// Деструктор класу GameObject, видаляє всі компоненти та знімає реєстрацію об'єкта
	virtual ~Entity();

	// Повертає вказівник на компонент Transform цього об'єкта
	Transform* getTransform();
	// Знищує об'єкт
	void destroy();

	// Створює новий компонент типу T і додає його до об'єкта
	template<typename T, typename... Args>
	T* addComponent(Args&&... args);

	// Повертає вказівник на компонент типу T, якщо він існує
	template<typename T>
	T* getComponent();

	// Повертає список вказівників на компоненти типу T, якщо вони існують
	template<typename T>
	std::list<T*> getComponents();

	// Видаляє компонент з об'єкта, якщо він існує
	bool removeComponent(Component* component);

	// Створює копію об'єкта разом з усіма його компонентами
	Entity* instantiate();

	// Повертає вказівник на батьківський об'єкт
	Entity* getParent();
	// Встановлює батьківський об'єкт для цього об'єкта
	void setParent(Entity* parent);

	// Перевіряє, чи об'єкт активний
	bool isActive();

	bool dontDestroyOnLoad = false;
	bool isActiveSelf = true;

protected:
	// Чи повинен об'єкт прокидати компоненти при створенні
	virtual bool shouldAwakeComponents() const { return true; }

	Transform mTransform;
	RigidBody* mRigidBody = nullptr;

	std::list<Component*> mComponents;

	Entity* mParent = nullptr;
	std::list<Entity*> mChildren;

	friend class Transform;
};

// Створює новий компонент типу T і додає його до об'єкта
template<typename T, typename... Args>
T* Entity::addComponent(Args&&... args)
{
	// Перевіряє, чи T є нащадком класу Component
	static_assert(std::is_base_of<Component, T>::value, "addComponent<T>: T must inherit from Component");

	if constexpr (std::is_base_of<RigidBody, T>::value) {
		if (mRigidBody) {
			return mRigidBody;
		}
	}

	// Створює новий компонент типу T з переданими аргументами
	auto* comp = new T(std::forward<Args>(args)...);

	// Встановлює власника компонента
	comp->setOwner(this);

	mComponents.push_back(comp);

	// Якщо компонент є фізичним тілом, зберігає його вказівник
	if constexpr (std::is_base_of<RigidBody, T>::value) {
		auto* rb = static_cast<RigidBody*>(comp);
		mRigidBody = rb;
	}

	static_cast<Component*>(comp)->registerComponent();

	// Якщо не є образом сутності, прокидає компонент
	if (shouldAwakeComponents())
	{
		static_cast<Component*>(comp)->awake();
	}

	return comp;
}

// Повертає вказівник на компонент типу T, якщо він існує
template<typename T>
T* Entity::getComponent()
{
	static_assert(std::is_base_of<Component, T>::value, "getComponent<T>: T must inherit from Component");

	for (auto* compPtr : mComponents)
	{
		if (auto casted = dynamic_cast<T*>(compPtr))
			return casted;
	}
	return nullptr;
}

// Повертає список вказівників на компоненти типу T, якщо вони існують
template<typename T>
std::list<T*> Entity::getComponents()
{
	static_assert(std::is_base_of<Component, T>::value, "getComponents<T>: T must inherit from Component");

	std::list<T*> results;

	for (auto* compPtr : mComponents)
	{
		if (auto casted = dynamic_cast<T*>(compPtr))
			results.push_back(casted);
	}
	return results;
}
