#include "ComponentManager.h"
#include "Component.h"
#include "RenderObject.h"
#include "Camera.h"

ComponentManager::ComponentManager()
{
}

ComponentManager::~ComponentManager()
{
}

void ComponentManager::registerComponent(Component* component)
{
	mComponents.push_back(component);
}

void ComponentManager::unregisterComponent(Component* component)
{
	mComponents.remove(component);
}

void ComponentManager::registerRenderer(RenderObject* renderer)
{
	mRenderers.push_back(renderer);
}

void ComponentManager::unregisterRenderer(RenderObject* renderer)
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
