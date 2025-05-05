#include "Component.h"
#include "GraphicsEngine.h"
#include "ComponentManager.h"

Component::Component()
{
    GraphicsEngine::get()->getComponentManager()->registerComponent(this);
}

Component::~Component()
{
    GraphicsEngine::get()->getComponentManager()->unregisterComponent(this);
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
