#pragma once
#include "Collider.h"

class PlaneCollider : public Collider
{
public:
	PlaneCollider();
	~PlaneCollider() override;

	void* getGeometry(Vector3& scale, bool convex = true) override;

protected:
	PlaneCollider* instantiate() const override
	{
		return new PlaneCollider(*this);
	}
};

