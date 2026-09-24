#pragma once
#include "Component.h"
#include <list>

class Prefab;
class Entity;

class ObjectSpawner : public Component
{
public:
	COMPONENT_TYPE(ObjectSpawner)

	ObjectSpawner(Prefab* prefab);
	~ObjectSpawner();

	// Записує власні поля та посилання у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює власні поля та посилання з файлу сцени
	void deserialize(const SceneReader& reader) override;

protected:
	ObjectSpawner* instantiate() const override
	{
		return new ObjectSpawner(*this);
	}

private:
	void update() override;

	void spawnObjects(int count, float range);

	// Створені під час гри об'єкти вже знищено редактором, тому список слід очистити
	void onEditorStop() override;

	Prefab* mPrefab = nullptr;
	int mCount = 100;
	int mObjectCount = 0;
	float mRange = 10.0f;

	std::list<Entity*> mSpawnedObjects = {};
};

