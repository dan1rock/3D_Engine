#pragma once
#include <list>

class Component;
class RenderObject;
class Camera;
class GameObject;

class ComponentManager
{
public:
	ComponentManager();
	~ComponentManager();

	void registerGameObject(GameObject* gameObject);
	void unregisterGameObject(GameObject* gameObject);
	void registerComponent(Component* component);
	void unregisterComponent(Component* component);
	void registerRenderer(RenderObject* renderer);
	void unregisterRenderer(RenderObject* renderer);
	void registerCamera(Camera* camera);
	void unregisterCamera(Camera* camera);

	void updateComponents();
	void updateRenderers();
	void updateCameras();
private:
	std::list<GameObject*> mGameObjects = {};
	std::list<Component*> mComponents = {};
	std::list<RenderObject*> mRenderers = {};
	std::list<Camera*> mCameras = {};
};

