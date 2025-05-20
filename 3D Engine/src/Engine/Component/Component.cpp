#include "Component.h"
#include "EntityManager.h"

Component::Component()
{
}

Component::~Component()
{
    EntityManager::get()->unregisterComponent(this);
}

GameObject* Component::getOwner()
{
    return mOwner;
}

void Component::registerComponent()
{
    EntityManager::get()->registerComponent(this);
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
