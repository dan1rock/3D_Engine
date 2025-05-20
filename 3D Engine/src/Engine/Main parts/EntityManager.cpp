#include "EntityManager.h"
#include "Component.h"
#include "GameObject.h"
#include "RenderComponent.h"
#include "Camera.h"
#include "GraphicsEngine.h"
#include "PhysicsEngine.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "ConvexMeshManager.h"
#include "Material.h"
#include "RigidBody.h"

EntityManager::EntityManager()
{
}

EntityManager::~EntityManager()
{
}

void EntityManager::registerGameObject(GameObject* gameObject)
{
	mGameObjects.push_back(gameObject);
}

void EntityManager::unregisterGameObject(GameObject* gameObject)
{
	mGameObjects.remove(gameObject);
}

void EntityManager::registerComponent(Component* component)
{
	mComponents.push_back(component);
}

void EntityManager::unregisterComponent(Component* component)
{
	mComponents.remove(component);
}

void EntityManager::registerRenderer(RenderComponent* renderer)
{
	mRenderers.push_back(renderer);
}

void EntityManager::unregisterRenderer(RenderComponent* renderer)
{
	mRenderers.remove(renderer);
}

void EntityManager::registerCamera(Camera* camera)
{
	mCameras.push_back(camera);
}

void EntityManager::unregisterCamera(Camera* camera)
{
	mCameras.remove(camera);
}

void EntityManager::registerMaterial(Material* material)
{
	mMaterials.push_back(material);
}

void EntityManager::unregisterMaterial(Material* material)
{
	mMaterials.remove(material);
}

void EntityManager::registerRigidBody(RigidBody* rigidBody)
{
	if (rigidBody->mActor) {
		mRigidBodies[rigidBody->mActor] = rigidBody;
	}
}

void EntityManager::unregisterRigidBody(RigidBody* rigidBody)
{
	if (rigidBody->mActor) {
		mRigidBodies.erase(rigidBody->mActor);
	}
}

RigidBody* EntityManager::getRigidBody(void* actor)
{
	auto it = mRigidBodies.find(actor);
	if (it != mRigidBodies.end()) {
		return it->second;
	}

	return nullptr;
}

void EntityManager::updateComponents()
{
	for (auto* c : mComponents) {
		if (!c->getOwner()->isActive) continue;
		c->update();
	}
}

void EntityManager::fixedUpdateComponents()
{
	for (auto* c : mComponents) {
		if (!c->getOwner()->isActive) continue;
		c->fixedUpdate();
	}
}

void EntityManager::updateRenderers()
{
	for (auto* r : mRenderers) {
		if (!r->getOwner()->isActive) continue;
		r->render();
	}
}

void EntityManager::updateCameras()
{
	for (auto* c : mCameras) {
		if (!c->getOwner()->isActive) continue;
		c->updateCamera();
	}
}

void EntityManager::onSceneLoadStart()
{
	auto it = mGameObjects.begin();
	while (it != mGameObjects.end())
	{
		GameObject* g = *it++;
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

void EntityManager::onSceneLoadFinished()
{
	GraphicsEngine::get()->getTextureManager()->unloadUnusedResources();
	GraphicsEngine::get()->getMeshManager()->unloadUnusedResources();
	PhysicsEngine::get()->getConvexMeshManager()->unloadUnusedResources();
}

EntityManager* EntityManager::get()
{
	static EntityManager instance;
	return &instance;
}
