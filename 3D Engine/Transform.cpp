#include "Transform.h"
#include "RigidBody.h"
#include "GameObject.h"

Transform::Transform()
{
	mMatrix.setIdentity();
}

Transform::~Transform()
{
}

Matrix* Transform::getMatrix()
{
	return &mMatrix;
}

Vector3 Transform::getPosition()
{
	return mPosition;
}

Vector3 Transform::getScale()
{
	return mScale;
}

Vector3 Transform::getRotation()
{
	return mRotation;
}

Vector3 Transform::getForward()
{
	return mMatrix.getZDirection().normalized();
}

void Transform::setPosition(Vector3 position, bool updateRb)
{
	mMatrix.setTranslation(position);
	mPosition = position;

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::setScale(Vector3 scale)
{
	mMatrix.setScale(scale);
	mScale = scale;
}

void Transform::setRotation(Vector3 rotation, bool updateRb)
{
	mMatrix.setRotation(rotation);
	mRotation = rotation;

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::setForward(Vector3 forward, bool updateRb)
{
	mMatrix.setForward(forward);
	mRotation = mMatrix.getRotation();

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}
