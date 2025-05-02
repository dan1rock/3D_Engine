#include "GameObject.h"
#include "ComponentManager.h"
#include "MeshRenderer.h"

GameObject::GameObject()
{
}

GameObject::GameObject(Vector3 position)
{
	mTransform.setPosition(position);
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

GameObject::~GameObject()
{
}

Transform* GameObject::getTransform()
{
    return &mTransform;
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

