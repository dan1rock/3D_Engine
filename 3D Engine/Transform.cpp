#include "Transform.h"

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
	return mMatrix.getTranslation();
}

Vector3 Transform::getScale()
{
	return mMatrix.getScale();
}

Vector3 Transform::getRotation()
{
	return Vector3();
}

void Transform::setPosition(Vector3 position)
{
	mMatrix.setTranslation(position);
}

void Transform::setScale(Vector3 scale)
{
	mMatrix.setScale(scale);
}

void Transform::setRotation(Vector3 rotation)
{
}
