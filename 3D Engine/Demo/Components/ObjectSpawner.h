#pragma once
#include "Component.h"

class Prefab;

class ObjectSpawner : public Component
{
public:
	ObjectSpawner(Prefab* prefab, int count);
	~ObjectSpawner();

protected:
	ObjectSpawner* instantiate() const override
	{
		return new ObjectSpawner(*this);
	}

private:
	void awake() override;

	Prefab* mPrefab = nullptr;
	int mCount = 0;
};

