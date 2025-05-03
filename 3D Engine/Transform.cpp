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

void Transform::setPosition(Vector3 position)
{
	mMatrix.setTranslation(position);
	mPosition = position;
}

void Transform::setScale(Vector3 scale)
{
	mMatrix.setScale(scale);
	mScale = scale;
}

void Transform::setRotation(Vector3 rotation)
{
	mMatrix.setRotation(rotation);
	mRotation = rotation;
}
