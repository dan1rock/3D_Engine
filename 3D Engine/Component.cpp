#include "Component.h"
#include "ComponentManager.h"

Component::Component()
{
    ComponentManager::get()->registerComponent(this);
}

Component::~Component()
{
    ComponentManager::get()->unregisterComponent(this);
}

GameObject* Component::getOwner()
{
    return mOwner;
}

void Component::setOwner(GameObject* gameObject)
{
    mOwner = gameObject;
}

void Component::update()
{
}

void Component::fixedUpdate()
{
}

void Component::awake()
{
}
