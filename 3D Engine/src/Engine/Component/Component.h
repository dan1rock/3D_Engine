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

	virtual Component* instantiate() const = 0;
	virtual void registerComponent();

private:
	void setOwner(GameObject* gameObject);

	virtual void awake();
	virtual void update();
	virtual void fixedUpdate();

	friend class GameObject;
	friend class EntityManager;
};

