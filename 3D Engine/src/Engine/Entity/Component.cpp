#include "Component.h"
#include "EntityManager.h"

Component::Component()
{
}

// Автоматично знімає реєстрацію компонента в EntityManager
Component::~Component()
{
    EntityManager::get()->unregisterComponent(this);
}

// Повертає вказівник на об'єкт-власник (GameObject) цього компонента
Entity* Component::getOwner()
{
    return mOwner;
}

// Реєструє компонент в EntityManager
void Component::registerComponent()
{
    EntityManager::get()->registerComponent(this);
}

// Встановлює власника (GameObject) для цього компонента
void Component::setOwner(Entity* gameObject)
{
    mOwner = gameObject;
}

// Викликається при активації компонента
void Component::awake()
{
}

// Оновлення компонента (викликається кожен кадр)
void Component::update()
{
}

// Фіксоване оновлення компонента (викликається з фіксованим кроком часу)
void Component::fixedUpdate()
{
}
