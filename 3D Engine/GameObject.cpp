#include "GameObject.h"
#include "ComponentManager.h"
#include "MeshRenderer.h"

GameObject::GameObject()
{
	ComponentManager::get()->registerGameObject(this);
	mTransform.setPosition(Vector3(0.0f, 0.0f, 0.0f));
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

GameObject::GameObject(Vector3 position)
{
    ComponentManager::get()->registerGameObject(this);
	mTransform.setPosition(position);
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

GameObject::~GameObject()
{
    ComponentManager::get()->unregisterGameObject(this);
}

Transform* GameObject::getTransform()
{
    return &mTransform;
}

void GameObject::destroy()
{
	mComponents.clear();
    delete this;
}

bool GameObject::removeComponent(Component* component)
{
    auto it = std::find_if(
        mComponents.begin(),
        mComponents.end(),
        [component](const std::unique_ptr<Component>& ptr) {
            return ptr.get() == component;
        });

    if (it != mComponents.end()) {
        mComponents.erase(it);
        return true;
    }
    return false;
}

