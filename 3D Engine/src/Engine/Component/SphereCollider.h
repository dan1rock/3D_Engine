#pragma once
#include "Collider.h"

// Компонент, що представляє сферичний коллайдер
class SphereCollider : public Collider
{
public:
	SphereCollider();
	SphereCollider(float radius);
	~SphereCollider() override;

	float radius = 1.0f;

	// Повертає вказівник на геометрію коллайдера
	void* getGeometry(Vector3& scale, bool convex = true) override;

protected:
	// Конструктор копіювання
	SphereCollider* instantiate() const override
	{
		return new SphereCollider(*this);
	}
};

