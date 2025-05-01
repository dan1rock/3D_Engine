#pragma once
#include "Transform.h"
#include <list>

class GameObject
{
public:
	GameObject();
	~GameObject();

	Transform* getTransform();

	template<typename T, typename... Args>
	T* addComponent(Args&&... args);
	bool removeComponent(Component* component);

private:
	Transform mTransform;

	std::list<std::unique_ptr<Component>> mComponents;
};

template<typename T, typename... Args>
T* GameObject::addComponent(Args&&... args)
{
	static_assert(std::is_base_of<Component, T>::value, "addComponent<T>: T must inherit from Component");

	auto comp = std::make_unique<T>(std::forward<Args>(args)...);

	comp->setOwner(this);

	T* ptr = comp.get();

	mComponents.push_back(std::move(comp));

	return ptr;
}
