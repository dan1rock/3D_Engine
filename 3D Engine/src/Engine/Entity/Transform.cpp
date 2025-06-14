#include "Transform.h"
#include "RigidBody.h"
#include "Entity.h"

// Ініціалізує модельну матрицю як одиничну
Transform::Transform()
{
	mMatrix.setIdentity();
	mLocalMatrix.setIdentity();
}

Transform::~Transform()
{
}

// Повертає вказівник на матрицю трансформації
Matrix* Transform::getMatrix()
{
	return &mMatrix;
}

Matrix* Transform::getLocalMatrix()
{
	return &mLocalMatrix;
}

// Повертає позицію об'єкта
Vector3 Transform::getPosition()
{
	return mPosition;
}

Vector3 Transform::getLocalPosition()
{
	return mLocalPosition;
}

// Повертає масштаб об'єкта
Vector3 Transform::getScale()
{
	return mScale;
}

Vector3 Transform::getLocalScale()
{
	return mLocalScale;
}

// Повертає обертання об'єкта
Vector3 Transform::getRotation()
{
	return mRotation;
}

Vector3 Transform::getLocalRotation()
{
	return mLocalRotation;
}

// Повертає напрямок "вперед" (Z) у світових координатах
Vector3 Transform::getForward()
{
	return mMatrix.getZDirection().normalized();
}

// Повертає напрямок "вправо" (X) у світових координатах
Vector3 Transform::getRight()
{
	return mMatrix.getXDirection().normalized();
}

// Повертає напрямок "вгору" (Y) у світових координатах
Vector3 Transform::getUp()
{
	return mMatrix.getYDirection().normalized();
}

void Transform::setMatrix(const Matrix& matrix, bool updateRb)
{
	mMatrix = matrix;
	mPosition = mMatrix.getTranslation();
	mScale = mMatrix.getScale();
	mRotation = mMatrix.getRotation();

	updateChildren(updateRb);

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

// Встановлює позицію об'єкта, за потреби оновлює фізичне тіло
void Transform::setPosition(Vector3 position, bool updateRb)
{
	mMatrix.setTranslation(position);
	mPosition = position;

	updateChildren(updateRb);

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::setLocalPosition(Vector3 position, bool updateRb)
{
	mLocalMatrix.setTranslation(position);
	mLocalPosition = position;

	updateGlobalMatrix(updateRb);
}

// Встановлює масштаб об'єкта
void Transform::setScale(Vector3 scale)
{
	mMatrix.setScale(scale);
	mScale = scale;

	updateChildren(true);

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::setLocalScale(Vector3 scale)
{
	mLocalMatrix.setScale(scale);
	mLocalScale = scale;

	updateGlobalMatrix(true);
}

// Встановлює обертання об'єкта, за потреби оновлює фізичне тіло
void Transform::setRotation(Vector3 rotation, bool updateRb)
{
	mMatrix.setRotation(rotation);
	mRotation = rotation;

	updateChildren(updateRb);

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::setLocalRotation(Vector3 rotation, bool updateRb)
{
	mLocalMatrix.setRotation(rotation);
	mLocalRotation = rotation;

	updateGlobalMatrix(updateRb);
}

// Встановлює напрямок "вперед" (Z) для об'єкта, за потреби оновлює фізичне тіло
void Transform::setForward(Vector3 forward, bool updateRb)
{
	mMatrix.setForward(forward);
	mRotation = mMatrix.getRotation();

	updateChildren(updateRb);

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::updateGlobalMatrix(bool updateRb)
{
	if (mOwner->mParent)
	{
		mMatrix = mLocalMatrix;
		mMatrix *= *mOwner->mParent->getTransform()->getMatrix();
	}
	else
	{
		mMatrix = mLocalMatrix;
	}

	mPosition = mMatrix.getTranslation();
	mScale = mMatrix.getScale();
	mRotation = mMatrix.getRotation();

	updateChildren(updateRb);

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

void Transform::updateChildren(bool updateRb)
{
	for (Entity* child : mOwner->mChildren)
	{
		if (child->getTransform())
		{
			child->getTransform()->updateGlobalMatrix(updateRb);
		}
	}
}
