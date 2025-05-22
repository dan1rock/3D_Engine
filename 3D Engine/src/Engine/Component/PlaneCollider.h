#pragma once
#include "Collider.h"

// Компонент, що представляє коллайдер площини в ігровому об'єкті
class PlaneCollider : public Collider
{
public:
	PlaneCollider();
	~PlaneCollider() override;

	// Повертає вказівник на геометрію коллайдера
	void* getGeometry(Vector3& scale, bool convex = true) override;

protected:
	// Конструктор копіювання
	PlaneCollider* instantiate() const override
	{
		return new PlaneCollider(*this);
	}
};

