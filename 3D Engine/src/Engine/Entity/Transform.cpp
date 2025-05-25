#include "Transform.h"
#include "RigidBody.h"
#include "Entity.h"

// Ініціалізує модельну матрицю як одиничну
Transform::Transform()
{
	mMatrix.setIdentity();
}

Transform::~Transform()
{
}

// Повертає вказівник на матрицю трансформації
Matrix* Transform::getMatrix()
{
	return &mMatrix;
}

// Повертає позицію об'єкта
Vector3 Transform::getPosition()
{
	return mPosition;
}

// Повертає масштаб об'єкта
Vector3 Transform::getScale()
{
	return mScale;
}

// Повертає обертання об'єкта
Vector3 Transform::getRotation()
{
	return mRotation;
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

// Встановлює позицію об'єкта, за потреби оновлює фізичне тіло
void Transform::setPosition(Vector3 position, bool updateRb)
{
	mMatrix.setTranslation(position);
	mPosition = position;

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

// Встановлює масштаб об'єкта
void Transform::setScale(Vector3 scale)
{
	mMatrix.setScale(scale);
	mScale = scale;
}

// Встановлює обертання об'єкта, за потреби оновлює фізичне тіло
void Transform::setRotation(Vector3 rotation, bool updateRb)
{
	mMatrix.setRotation(rotation);
	mRotation = rotation;

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}

// Встановлює напрямок "вперед" (Z) для об'єкта, за потреби оновлює фізичне тіло
void Transform::setForward(Vector3 forward, bool updateRb)
{
	mMatrix.setForward(forward);
	mRotation = mMatrix.getRotation();

	if (!updateRb) return;

	if (RigidBody* rb = getOwner()->mRigidBody) {
		rb->updateGlobalPose();
	}
}
