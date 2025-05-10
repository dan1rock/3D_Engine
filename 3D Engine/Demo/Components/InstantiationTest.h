#pragma once
#include "Component.h"
#include "Vector3.h"
#include <list>

class GameObject;
class Prefab;
class Material;

class InstantiationTest : public Component
{
public:
	InstantiationTest();
	~InstantiationTest() override;

	Prefab* prefab = nullptr;

protected:
	InstantiationTest* instantiate() const override {
		return new InstantiationTest(*this);
	};

private:
	void update() override;

	std::list<GameObject*> mGameObjects = {};

	Vector3 mPosition = { 0.0f, 0.0f, 0.0f };
};

