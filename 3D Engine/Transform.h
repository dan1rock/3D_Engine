#pragma once
#include "Component.h"
#include "Matrix.h"

class Transform : public Component
{
public:
	Transform();
	~Transform() override;

	Matrix* getMatrix();
	Vector3 getPosition();
	Vector3 getScale();
	Vector3 getRotation();
	Vector3 getForward();

	void setPosition(Vector3 position, bool updateRb = true);
	void setScale(Vector3 scale);
	void setRotation(Vector3 rotation, bool updateRb = true);
	void setForward(Vector3 forward, bool updateRb = true);

protected:
	Transform* instantiate() const override
	{
		return new Transform(*this);
	}

private:
	Matrix mMatrix = {};

	Vector3 mPosition = {};
	Vector3 mScale = { 1.0f, 1.0f, 1.0f };
	Vector3 mRotation = { 0.0f, 0.0f, 0.0f };
};

