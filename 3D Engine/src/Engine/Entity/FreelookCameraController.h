#pragma once
#include "Component.h"

// Компонент для управління камерою в стилі "вільного погляду" (freelook)
class FreelookCameraController : public Component
{
public:
	FreelookCameraController();
	FreelookCameraController(float speed, float mouseSpeed);
	~FreelookCameraController() override;

protected:
	// Оновлює стан камери в стилі "вільного погляду"
	void update() override;

	float speed = 2.0f;
	float speedIncrement = 1.0f;
	float mouseSpeed = 0.002f;
};

