#pragma once
#include "Transform.h"
#include <list>

class RigidBody;

// Представляє об'єкт у сцені, що може мати компоненти
class GameObject
{
public:
	// Конструктор класу GameObject, реєструє об'єкт у EntityManager та ініціалізує трансформацію за замовчуванням
	GameObject();
	// Конструктор класу GameObject з початковою позицією
	GameObject(Vector3 position);
	// Деструктор класу GameObject, видаляє всі компоненти та знімає реєстрацію об'єкта
	virtual ~GameObject();

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
	GameObject* instantiate();

	bool dontDestroyOnLoad = false;
	bool isActive = true;

protected:
	// Чи повинен об'єкт прокидати компоненти при створенні
	virtual bool shouldAwakeComponents() const { return true; }

	Transform mTransform;
	RigidBody* mRigidBody = nullptr;

	std::list<Component*> mComponents;

	friend class Transform;
};

// Створює новий компонент типу T і додає його до об'єкта
template<typename T, typename... Args>
T* GameObject::addComponent(Args&&... args)
{
	static_assert(std::is_base_of<Component, T>::value, "addComponent<T>: T must inherit from Component");

	if constexpr (std::is_base_of<RigidBody, T>::value) {
		if (mRigidBody) {
			return mRigidBody;
		}
	}

	auto* comp = new T(std::forward<Args>(args)...);

	comp->setOwner(this);

	mComponents.push_back(comp);

	if constexpr (std::is_base_of<RigidBody, T>::value) {
		auto* rb = static_cast<RigidBody*>(comp);
		mRigidBody = rb;
	}

	static_cast<Component*>(comp)->registerComponent();

	if (shouldAwakeComponents())
	{
		static_cast<Component*>(comp)->awake();
	}

	return comp;
}

// Повертає вказівник на компонент типу T, якщо він існує
template<typename T>
T* GameObject::getComponent()
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
std::list<T*> GameObject::getComponents()
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
