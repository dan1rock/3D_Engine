#pragma once
#include <list>

class Component;
class RenderObject;

class ComponentManager
{
public:
	ComponentManager();
	~ComponentManager();

	void registerComponent(Component* component);
	void unregisterComponent(Component* component);
	void registerRenderer(RenderObject* renderer);
	void unregisterRenderer(RenderObject* renderer);

	void updateComponents();
	void updateRenderers();
private:
	std::list<Component*> mComponents = {};
	std::list<RenderObject*> mRenderers = {};
};

