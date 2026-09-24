#pragma once
#include "Component.h"
#include "Vector3.h"
#include <list>

class Entity;
class Prefab;
class Material;

class InstantiationTest : public Component
{
public:
	COMPONENT_TYPE(InstantiationTest)

	InstantiationTest();
	~InstantiationTest() override;

	Prefab* prefab = nullptr;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	InstantiationTest* instantiate() const override {
		return new InstantiationTest(*this);
	};

private:
	void update() override;

	std::list<Entity*> mGameObjects = {};

	Vector3 mPosition = { 0.0f, 0.0f, 0.0f };
};

