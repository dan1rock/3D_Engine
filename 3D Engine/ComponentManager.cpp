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

void ComponentManager::updateComponents()
{
	for (auto* c : mComponents) {
		c->update();
	}
}

void ComponentManager::fixedUpdateComponents()
{
	for (auto* c : mComponents) {
		c->fixedUpdate();
	}
}

void ComponentManager::updateRenderers()
{
	for (auto* r : mRenderers) {
		r->render();
	}
}

void ComponentManager::updateCameras()
{
	for (auto* c : mCameras) {
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
