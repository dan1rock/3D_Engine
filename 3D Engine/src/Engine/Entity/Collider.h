#pragma once
#include "Component.h"
#include "Vector3.h"

// Компонент, що представляє коллайдер в ігровому об'єкті
class Collider : public Component
{
public:
	Collider();
	virtual ~Collider();

	// Повертає вказівник на геометрію коллайдера
	virtual void* getGeometry(Vector3& scale, bool convex = true) = 0;

protected:
	// Додає коллайдер до фізичного тіла
	void awake() override;

	void* mGeometry = nullptr;

	Vector3 mScale = {};
};

