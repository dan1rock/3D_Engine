#pragma once
#include "FreelookCameraController.h"

class Material;
class Prefab;

class DemoPlayer : public FreelookCameraController
{
public:
	COMPONENT_TYPE(DemoPlayer)

	DemoPlayer(float speed, float sensitivity);
	~DemoPlayer() override;

	Prefab* projectilePrefab = nullptr;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

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

