#pragma once
#include "Component.h"
#include <list>

class Prefab;
class Entity;

class ObjectSpawner : public Component
{
public:
	ObjectSpawner(Prefab* prefab);
	~ObjectSpawner();

protected:
	ObjectSpawner* instantiate() const override
	{
		return new ObjectSpawner(*this);
	}

private:
	void update() override;

	void spawnObjects(int count, float range);

	Prefab* mPrefab = nullptr;
	int mCount = 100;
	int mObjectCount = 0;
	float mRange = 10.0f;

	std::list<Entity*> mSpawnedObjects = {};
};

