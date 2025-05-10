#include "Prefab.h"
#include "ComponentManager.h"

Prefab::Prefab()
{
	isActive = false;
}

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
