#pragma once
#include "Transform.h"
#include <list>

class RigidBody;

class GameObject
{
public:
	GameObject();
	GameObject(Vector3 position);
	~GameObject();

	Transform* getTransform();
	void destroy();

	template<typename T, typename... Args>
	T* addComponent(Args&&... args);

	template<typename T>
	T* getComponent();

	bool removeComponent(Component* component);

private:
	Transform mTransform = {};
	RigidBody* mRigidBody = nullptr;

	std::list<std::unique_ptr<Component>> mComponents;
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

	auto comp = std::make_unique<T>(std::forward<Args>(args)...);

	comp->setOwner(this);

	T* ptr = comp.get();

	mComponents.push_back(std::move(comp));

	if constexpr (std::is_base_of<RigidBody, T>::value) {
		auto* rb = static_cast<RigidBody*>(ptr);
		mRigidBody = rb;
	}

	ptr->awake();

	return ptr;
}

template<typename T>
T* GameObject::getComponent()
{
	static_assert(std::is_base_of<Component, T>::value, "getComponent<T>: T must inherit from Component");

	for (auto& compPtr : mComponents)
	{
		if (auto casted = dynamic_cast<T*>(compPtr.get()))
			return casted;
	}
	return nullptr;
}
