#include "Collider.h"
#include "GameObject.h"
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
	if (RigidBody* rb = mOwner->getComponent<RigidBody>()) {
		rb->addCollider(this);
	}
}
