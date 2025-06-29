#include "Collider.h"
#include "Entity.h"
#include "RigidBody.h"

Collider::Collider()
{
}

Collider::~Collider()
{
	if (mGeometry)
	{
		delete mGeometry;
	}
}

// Додає коллайдер до фізичного тіла
void Collider::awake()
{
	mGeometry = nullptr;

	if (RigidBody* rb = mOwner->getComponent<RigidBody>()) {
		rb->addCollider(this);
	}
}
