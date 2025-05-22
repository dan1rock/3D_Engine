#pragma once
#include "Vector3.h"
#include <vector>

class RigidBody;
class Collider;

// Структура для зберігання інформації про результати raycast
struct RaycastHit
{
	float distance;
	Vector3 point;
	Vector3 normal;
	RigidBody* rigidBody;
	Collider* collider;
};

// Структура для зберігання інформації про результати sphere cast
struct OverlapHit
{
	RigidBody* rigidBody;
	Collider* collider;
};

class Physics
{
public:
	// Виконує фізичний raycast у сцені, ігноруючи заданий RigidBody
	static bool raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, RigidBody* ignore = nullptr);
	// Виконує фізичний sphere cast у сцені, ігноруючи заданий RigidBody
	static bool sphereCast(const Vector3& origin, const Vector3& direction, float radius, float maxDistance, RaycastHit& outHit, RigidBody* ignore = nullptr);
	// Перевіряє, які об'єкти знаходяться всередині сфери з заданим радіусом
	static bool overlapSphere(const Vector3& origin, float radius, std::vector<OverlapHit>& outHits);
};

