#pragma once
#include "Collider.h"

class SphereCollider : public Collider
{
public:
	SphereCollider();
	SphereCollider(float radius);
	~SphereCollider() override;

	float radius = 1.0f;

	void* getGeometry(Vector3& scale, bool convex = true) override;

protected:
	SphereCollider* instantiate() const override
	{
		return new SphereCollider(*this);
	}
};

