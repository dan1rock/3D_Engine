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

void Collider::awake()
{
	if (RigidBody* rb = mOwner->getComponent<RigidBody>()) {
		rb->addCollider(this);
	}
}
