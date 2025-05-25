#pragma once
#include "Collider.h"

class ConvexMesh;

// Компонент, що представляє коллайдер з мешу в ігровому об'єкті
class MeshCollider : public Collider
{
public:
	MeshCollider();
	~MeshCollider() override;

	// Повертає вказівник на геометрію коллайдера
	void* getGeometry(Vector3& scale, bool convex = true) override;

	// Встановлює меш для коллайдера з файлу
	void setConvexMesh(const wchar_t* fullPath);

protected:
	// Конструктор копіювання
	MeshCollider* instantiate() const override {
		return new MeshCollider(*this);
	}

private:
	ConvexMesh* mConvexMesh = nullptr;
};

