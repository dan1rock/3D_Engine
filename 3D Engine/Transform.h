#pragma once
#include "Component.h"
#include "Matrix.h"

class Transform : public Component
{
public:
	Transform();
	~Transform();

	Matrix* getMatrix();
	Vector3 getPosition();
	Vector3 getScale();
	Vector3 getRotation();

	void setPosition(Vector3 position);
	void setScale(Vector3 scale);
	void setRotation(Vector3 rotation);

private:
	Matrix mMatrix = {};

	Vector3 mPosition = {};
	Vector3 mScale = { 1.0f, 1.0f, 1.0f };
	Vector3 mRotation = { 0.0f, 0.0f, 0.0f };
};

