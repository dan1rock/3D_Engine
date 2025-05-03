#pragma once
#include "Component.h"
#include "Vector3.h"
#include <list>

class GameObject;
class Material;

class InstantiationTest : public Component
{
public:
	InstantiationTest();
	~InstantiationTest() override;

private:
	void update() override;

	std::list<GameObject*> mGameObjects = {};

	Vector3 mPosition = { 0.0f, 0.0f, 0.0f };

	Material* mMaterial = nullptr;
};

