#include "Prefab.h"
#include "EntityManager.h"

// Задання статусу активності об'єкта як неактивного
Prefab::Prefab()
{
	isActive = false;
}

// Задання статусу активності об'єкта як неактивного з позицією
Prefab::Prefab(Vector3 position)
{
	isActive = false;
	mTransform.setPosition(position);
	mTransform.setScale(Vector3(1.0f, 1.0f, 1.0f));
	mTransform.setRotation(Vector3(0.0f, 0.0f, 0.0f));
}

Prefab::~Prefab()
{
}
