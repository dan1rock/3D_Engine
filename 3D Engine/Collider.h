#pragma once
#include "Component.h"
#include "Vector3.h"

class Collider : public Component
{
public:
	Collider();
	virtual ~Collider();

	virtual void* getGeometry(Vector3& scale, bool convex = true) = 0;

protected:
	void awake() override;

	void* mGeometry = nullptr;

	Vector3 mScale = {};
};

