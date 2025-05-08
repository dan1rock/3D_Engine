#include "ComponentManager.h"
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

ComponentManager::ComponentManager()
{
}

ComponentManager::~ComponentManager()
{
}

void ComponentManager::registerGameObject(GameObject* gameObject)
{
	mGameObjects.push_back(gameObject);
}

void ComponentManager::unregisterGameObject(GameObject* gameObject)
{
	mGameObjects.remove(gameObject);
}

void ComponentManager::registerComponent(Component* component)
{
	mComponents.push_back(component);
}

void ComponentManager::unregisterComponent(Component* component)
{
	mComponents.remove(component);
}

void ComponentManager::registerRenderer(RenderComponent* renderer)
{
	mRenderers.push_back(renderer);
}

void ComponentManager::unregisterRenderer(RenderComponent* renderer)
{
	mRenderers.remove(renderer);
}

void ComponentManager::registerCamera(Camera* camera)
{
	mCameras.push_back(camera);
}

void ComponentManager::unregisterCamera(Camera* camera)
{
	mCameras.remove(camera);
}

void ComponentManager::registerMaterial(Material* material)
{
	mMaterials.push_back(material);
}

void ComponentManager::unregisterMaterial(Material* material)
{
	mMaterials.remove(material);
}

void ComponentManager::registerRigidBody(RigidBody* rigidBody)
{
	if (rigidBody->mActor) {
		mRigidBodies[rigidBody->mActor] = rigidBody;
	}
}

void ComponentManager::unregisterRigidBody(RigidBody* rigidBody)
{
	if (rigidBody->mActor) {
		mRigidBodies.erase(rigidBody->mActor);
	}
}

RigidBody* ComponentManager::getRigidBody(void* actor)
{
	auto it = mRigidBodies.find(actor);
	if (it != mRigidBodies.end()) {
		return it->second;
	}

	return nullptr;
}

void ComponentManager::updateComponents()
{
	for (auto* c : mComponents) {
		if (!c->getOwner()->isActive) continue;
		c->update();
	}
}

void ComponentManager::fixedUpdateComponents()
{
	for (auto* c : mComponents) {
		if (!c->getOwner()->isActive) continue;
		c->fixedUpdate();
	}
}

void ComponentManager::updateRenderers()
{
	for (auto* r : mRenderers) {
		if (!r->getOwner()->isActive) continue;
		r->render();
	}
}

void ComponentManager::updateCameras()
{
	for (auto* c : mCameras) {
		if (!c->getOwner()->isActive) continue;
		c->updateCamera();
	}
}

void ComponentManager::onSceneLoadStart()
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

void ComponentManager::onSceneLoadFinished()
{
	GraphicsEngine::get()->getTextureManager()->unloadUnusedResources();
	GraphicsEngine::get()->getMeshManager()->unloadUnusedResources();
	PhysicsEngine::get()->getConvexMeshManager()->unloadUnusedResources();
}

ComponentManager* ComponentManager::get()
{
	static ComponentManager instance;
	return &instance;
}
