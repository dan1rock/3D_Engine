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

	// Записує власні поля та посилання у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює власні поля та посилання з файлу сцени
	void deserialize(const SceneReader& reader) override;

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

