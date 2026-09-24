#include "SphereCollider.h"
#include <PxPhysicsAPI.h>
#include "SceneIO.h"

SphereCollider::SphereCollider()
{
}

SphereCollider::SphereCollider(float radius)
{
	this->radius = radius;
}

SphereCollider::~SphereCollider()
{
}

// Повертає вказівник на геометрію коллайдера
void* SphereCollider::getGeometry(Vector3& scale, bool convex)
{
	// Якщо геометрія вже існує і масштаб не змінився, повертаємо її
	if (mGeometry) {
		if (scale == mScale) {
			return mGeometry;
		}

		delete mGeometry;
		mGeometry = nullptr;
	}

	mScale = scale;

	// Створюємо сферичну геометрію з урахуванням масштабу
	float scaledRadius = radius * scale.x;

	mGeometry = new physx::PxSphereGeometry(scaledRadius);

    return mGeometry;
}

// Записує радіус коллайдера у файл сцени
void SphereCollider::serialize(SceneWriter& writer) const
{
	writer.write("radius", radius);
}

// Відновлює радіус коллайдера з файлу сцени
void SphereCollider::deserialize(const SceneReader& reader)
{
	radius = reader.read("radius", radius);
}
