#pragma once

class GameObject;

class Component
{
public:
	Component();
	~Component();

	GameObject* getOwner();

private:
	GameObject* mOwner = nullptr;

	void setOwner(GameObject* gameObject);
	virtual void update();

	friend class GameObject;
	friend class ComponentManager;
};

