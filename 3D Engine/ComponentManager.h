#pragma once
#include <list>

class Component;
class RenderComponent;
class Camera;
class GameObject;
class Material;

class ComponentManager
{
public:
	ComponentManager();
	~ComponentManager();

	void registerGameObject(GameObject* gameObject);
	void unregisterGameObject(GameObject* gameObject);
	void registerComponent(Component* component);
	void unregisterComponent(Component* component);
	void registerRenderer(RenderComponent* renderer);
	void unregisterRenderer(RenderComponent* renderer);
	void registerCamera(Camera* camera);
	void unregisterCamera(Camera* camera);
	void registerMaterial(Material* material);
	void unregisterMaterial(Material* material);

	void updateComponents();
	void fixedUpdateComponents();
	void updateRenderers();
	void updateCameras();

	void onSceneLoadStart();
	void onSceneLoadFinished();

	static ComponentManager* get();

private:
	std::list<GameObject*> mGameObjects = {};
	std::list<Component*> mComponents = {};
	std::list<RenderComponent*> mRenderers = {};
	std::list<Camera*> mCameras = {};
	std::list<Material*> mMaterials = {};
};

