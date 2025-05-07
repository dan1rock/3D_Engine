#pragma once
#include "Transform.h"
#include <list>

class RigidBody;

class GameObject
{
public:
	GameObject();
	GameObject(Vector3 position);
	virtual ~GameObject();

	Transform* getTransform();
	void destroy();

	template<typename T, typename... Args>
	T* addComponent(Args&&... args);

	template<typename T>
	T* getComponent();

	template<typename T>
	std::list<T*> getComponents();

	bool removeComponent(Component* component);

	GameObject* instantiate();

	bool dontDestroyOnLoad = false;
	bool isActive = true;

protected:
	virtual bool shouldAwakeComponents() const { return true; }

	Transform mTransform;
	RigidBody* mRigidBody = nullptr;

	std::list<Component*> mComponents;

	friend class Transform;
};

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
