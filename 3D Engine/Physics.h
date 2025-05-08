#pragma once
#include "Vector3.h"
#include <vector>

class RigidBody;
class Collider;

struct RaycastHit
{
	float distance;
	Vector3 point;
	Vector3 normal;
	RigidBody* rigidBody;
	Collider* collider;
};

struct OverlapHit
{
	RigidBody* rigidBody;
	Collider* collider;
};

class Physics
{
public:
	static bool raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, RigidBody* ignore = nullptr);
	static bool sphereCast(const Vector3& origin, const Vector3& direction, float radius, float maxDistance, RaycastHit& outHit, RigidBody* ignore = nullptr);
	static bool overlapSphere(const Vector3& origin, float radius, std::vector<OverlapHit>& outHits);
};

