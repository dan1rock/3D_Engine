#pragma once
#include "FreelookCameraController.h"

class Material;
class Prefab;

class DemoPlayer : public FreelookCameraController
{
public:
	DemoPlayer(float speed, float sensitivity);
	~DemoPlayer() override;

	Prefab* projectilePrefab = nullptr;

protected:
	DemoPlayer* instantiate() const override
	{
		return new DemoPlayer(*this);
	}

private:
	void awake() override;
	void update() override;

	Material* mMaterial = nullptr;
};

