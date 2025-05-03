#pragma once
#include "Component.h"

class FreelookCameraController : public Component
{
public:
	FreelookCameraController();
	FreelookCameraController(float speed, float mouseSpeed);
	~FreelookCameraController() override;

private:
	void update() override;

	float speed = 2.0f;
	float mouseSpeed = 0.002f;
};

