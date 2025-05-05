#pragma once
#include "FreelookCameraController.h"

class Material;

class DemoPlayer : public FreelookCameraController
{
public:
	DemoPlayer(float speed, float sensitivity);
	~DemoPlayer() override;

private:
	void awake() override;
	void update() override;

	Material* mMaterial = nullptr;
};

