#include "EntityManager.h"
#include "Component.h"
#include "Entity.h"
#include "Renderer.h"
#include "Camera.h"
#include "GraphicsEngine.h"
#include "PhysicsEngine.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "ConvexMeshManager.h"
#include "Material.h"
#include "RigidBody.h"

#include <iostream>

// Реєструє ігровий об'єкт у менеджері
void EntityManager::registerEntity(Entity* gameObject)
{
	mEntities.push_back(gameObject);
}

// Видаляє ігровий об'єкт з менеджера
void EntityManager::unregisterEntity(Entity* gameObject)
{
	mEntities.remove(gameObject);
}

// Реєструє компонент у менеджері
void EntityManager::registerComponent(Component* component)
{
	mComponents.push_back(component);
}

// Видаляє компонент з менеджера
void EntityManager::unregisterComponent(Component* component)
{
	mComponents.remove(component);
}

// Реєструє рендер-компонент у менеджері
void EntityManager::registerRenderer(Renderer* renderer)
{
	mRenderers.push_back(renderer);
}

// Видаляє рендер-компонент з менеджера
void EntityManager::unregisterRenderer(Renderer* renderer)
{
	mRenderers.remove(renderer);
}

// Реєструє камеру у менеджері
void EntityManager::registerCamera(Camera* camera)
{
	mCameras.push_back(camera);
}

// Видаляє камеру з менеджера
void EntityManager::unregisterCamera(Camera* camera)
{
	mCameras.remove(camera);
}

// Реєструє матеріал у менеджері
void EntityManager::registerMaterial(Material* material)
{
	mMaterials.push_back(material);
}

// Видаляє матеріал з менеджера
void EntityManager::unregisterMaterial(Material* material)
{
	mMaterials.remove(material);
}

// Реєструє фізичне тіло у менеджері
void EntityManager::registerRigidBody(RigidBody* rigidBody)
{
	if (rigidBody->mActor) {
		mRigidBodies[rigidBody->mActor] = rigidBody;
	}
}

// Видаляє фізичне тіло з менеджера
void EntityManager::unregisterRigidBody(RigidBody* rigidBody)
{
	if (rigidBody->mActor) {
		mRigidBodies.erase(rigidBody->mActor);
	}
}

// Повертає фізичне тіло за вказаним актором
RigidBody* EntityManager::getRigidBody(void* actor)
{
	auto it = mRigidBodies.find(actor);
	if (it != mRigidBodies.end()) {
		return it->second;
	}

	return nullptr;
}

// Оновлює всі компоненти
void EntityManager::updateComponents()
{
	for (auto* c : mComponents) {
		if (!c->getOwner()->isActive()) continue;
		c->update();
	}
}

// Виконує фіксоване оновлення для всіх компонентів
void EntityManager::fixedUpdateComponents()
{
	for (auto* c : mComponents) {
		if (!c->getOwner()->isActive()) continue;
		c->fixedUpdate();
	}
}

// Оновлює всі рендер-компоненти
void EntityManager::updateRenderers()
{
	for (auto* r : mRenderers) {
		if (!r->getOwner()->isActive()) continue;
		r->render();
	}
}

// Оновлює всі камери
void EntityManager::updateCameras()
{
	for (auto* c : mCameras) {
		if (!c->getOwner()->isActive()) continue;
		c->updateCamera();
	}
}

// Викликається на початку завантаження сцени
void EntityManager::onSceneLoadStart()
{
	std::list<Entity*> toDestroy = mEntities;

	auto it = toDestroy.begin();
	while (it != toDestroy.end())
	{
		Entity* g = *it++;
		if (g->dontDestroyOnLoad)
			continue;

		g->destroy();
	}

	auto it2 = mMaterials.begin();
	while (it2 != mMaterials.end())
	{
		Material* m = *it2++;
		if (m->dontDeleteOnLoad)
			continue;
		delete m;
	}

	GraphicsEngine::get()->getTextureManager()->markResourcesAsUnused();
	GraphicsEngine::get()->getMeshManager()->markResourcesAsUnused();
	PhysicsEngine::get()->getConvexMeshManager()->markResourcesAsUnused();
}

// Викликається після завершення завантаження сцени
void EntityManager::onSceneLoadFinished()
{
	GraphicsEngine::get()->getTextureManager()->unloadUnusedResources();
	GraphicsEngine::get()->getMeshManager()->unloadUnusedResources();
	PhysicsEngine::get()->getConvexMeshManager()->unloadUnusedResources();
}

// Повертає єдиний екземпляр менеджера сутностей (синглтон)
EntityManager* EntityManager::get()
{
	static EntityManager instance;
	return &instance;
}
