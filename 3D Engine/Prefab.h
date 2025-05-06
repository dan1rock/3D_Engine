#pragma once
#include "GameObject.h"

class Prefab : public GameObject
{
public:
	Prefab();
	Prefab(Vector3 position);
	~Prefab() override;

protected:
	bool shouldAwakeComponents() const override {
		return false;
	}
};

