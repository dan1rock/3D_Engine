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

	// Записує власні поля та посилання у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює власні поля та посилання з файлу сцени
	void deserialize(const SceneReader& reader) override;

protected:
	InstantiationTest* instantiate() const override {
		return new InstantiationTest(*this);
	};

private:
	void update() override;

	std::list<Entity*> mGameObjects = {};

	Vector3 mPosition = { 0.0f, 0.0f, 0.0f };
};

