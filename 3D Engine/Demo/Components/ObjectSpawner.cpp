#include "ObjectSpawner.h"
#include "Prefab.h"
#include "MeshRenderer.h"
#include "Entity.h"
#include "Material.h"

#include <random>
#include <ctime>

static std::mt19937 rng{ static_cast<unsigned int>(std::time(nullptr)) };
static std::uniform_real_distribution<float> posDist(-10.0f, 10.0f);
static std::uniform_real_distribution<float> scaleDist(0.1f, 1.0f);
static std::uniform_real_distribution<float> rotDist(0.0f, 2.0f * 3.14159265359f);

ObjectSpawner::ObjectSpawner(Prefab* prefab, int count)
{
	mPrefab = prefab;
	mCount = count;
}

ObjectSpawner::~ObjectSpawner()
{
}

void ObjectSpawner::awake()
{
	if (!mPrefab) return;

	for (int i = 0; i < mCount; ++i)
	{
		Entity* entity = mPrefab->instantiate();
		if (entity)
		{
			entity->getTransform()->setScale(Vector3(1, 1, 1) * scaleDist(rng));
			entity->getTransform()->setPosition(Vector3(posDist(rng), posDist(rng) * 0.1f, posDist(rng)));
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
		}
	}
}
