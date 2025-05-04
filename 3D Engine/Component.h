#pragma once

class GameObject;

class Component
{
public:
	Component();
	virtual ~Component();

	GameObject* getOwner();

protected:
	GameObject* mOwner = nullptr;

private:
	void setOwner(GameObject* gameObject);

	virtual void awake();
	virtual void update();

	friend class GameObject;
	friend class ComponentManager;
};

