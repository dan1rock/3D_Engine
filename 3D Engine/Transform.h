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
};

