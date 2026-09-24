#pragma once
#include "Collider.h"

// Компонент, що представляє сферичний коллайдер
class SphereCollider : public Collider
{
public:
	COMPONENT_TYPE(SphereCollider)

	SphereCollider();
	SphereCollider(float radius);
	~SphereCollider() override;

	float radius = 1.0f;

	// Записує радіус коллайдера у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює радіус коллайдера з файлу сцени
	void deserialize(const SceneReader& reader) override;

	// Повертає вказівник на геометрію коллайдера
	void* getGeometry(Vector3& scale, bool convex = true) override;

protected:
	// Конструктор копіювання
	SphereCollider* instantiate() const override
	{
		return new SphereCollider(*this);
	}
};

