#include "ObjectSpawner.h"
#include "Prefab.h"
#include "MeshRenderer.h"
#include "Entity.h"
#include "Material.h"
#include "imgui.h"

#include <random>
#include <ctime>
#include "SceneIO.h"

static std::mt19937 rng{ static_cast<unsigned int>(std::time(nullptr)) };
static std::uniform_real_distribution<float> posDist(-1.0f, 1.0f);
static std::uniform_real_distribution<float> scaleDist(0.1f, 1.0f);
static std::uniform_real_distribution<float> rotDist(0.0f, 2.0f * 3.14159265359f);

ObjectSpawner::ObjectSpawner(Prefab* prefab)
{
	mPrefab = prefab;
}

ObjectSpawner::~ObjectSpawner()
{
}

void ObjectSpawner::update()
{
	if (!mPrefab) return;

	ImGui::Begin("Object Spawner", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::Button("Spawn Objects"))
	{
		spawnObjects(mCount, mRange);
	}

	ImGui::InputInt("Spawn Count", &mCount);

	ImGui::SliderFloat("Spawn Range", &mRange, 0.0f, 100.0f);

	ImGui::Text("Total Objects: %d", mObjectCount);

	if (ImGui::Button("Destroy Objects"))
	{
		for (auto& entity : mSpawnedObjects)
		{
			entity->destroy();
		}

		mSpawnedObjects.clear();
		mObjectCount = 0;
	}

	ImGui::End();
}

// Створені під час гри об'єкти вже знищено редактором, тому список слід очистити
void ObjectSpawner::onEditorStop()
{
	mSpawnedObjects.clear();
	mObjectCount = 0;
}

void ObjectSpawner::spawnObjects(int count, float range)
{
	for (int i = 0; i < count; ++i)
	{
		Entity* entity = mPrefab->instantiate();
		if (entity)
		{
			entity->getTransform()->setScale(Vector3(1, 1, 1) * scaleDist(rng));
			entity->getTransform()->setPosition(Vector3(posDist(rng) * range, posDist(rng) * range * 0.1f, posDist(rng) * range));
			entity->getTransform()->setRotation(Vector3(rotDist(rng), rotDist(rng), rotDist(rng)));

			if (auto* renderer = entity->getComponent<MeshRenderer>())
			{
				renderer->setMaterial(new Material(*renderer->getMaterial()));
				renderer->getMaterial()->setColor(
					scaleDist(rng),
					scaleDist(rng),
					scaleDist(rng),
					1.0f
				);
			}

			mSpawnedObjects.push_back(entity);
		}
	}

	mObjectCount += count;
}

// Записує власні поля та посилання у файл сцени
void ObjectSpawner::serialize(SceneWriter& writer) const
{
	writer.write("count", mCount);
	writer.write("range", mRange);
	writer.writeRef("prefab", mPrefab);
}

// Відновлює власні поля та посилання з файлу сцени
void ObjectSpawner::deserialize(const SceneReader& reader)
{
	mCount = reader.read("count", mCount);
	mRange = reader.read("range", mRange);
	mPrefab = reader.readPrefab("prefab");
}
