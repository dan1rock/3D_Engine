#pragma once
#include "Component.h"
#include "Vector3.h"

class RigidBody;
class Prefab;
class Transform;

class CarComponent : public Component
{
public:
	COMPONENT_TYPE(CarComponent)

	CarComponent();
	~CarComponent();

	int id = 0;

	float force = 1.0f;
	float damping = 0.1f;
	float maxDistance = 1.0f;
	float maxSpeed = 70.0f;
	float maxSteering = 0.3f;
	float gripRatio = 0.5f;

	Prefab* wheelPrefab = nullptr;

	// Записує власні поля та посилання у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює власні поля та посилання з файлу сцени
	void deserialize(const SceneReader& reader) override;

protected:
	CarComponent* instantiate() const override
	{
		return new CarComponent(*this);
	}

private:
	struct wheel
	{
		Transform* wheelTransform;
		Transform* positionTransform;
		bool rightSide;
		float spin;
		float spinSpeed;
		float acceleration;
		float springDistance;
	};

	void awake() override;
	void start();
	void update() override;
	void fixedUpdate() override;

	void simulateWheel(wheel& wheel);
	void initWheel(wheel& wheel, Vector3 position);

	RigidBody* mRigidBody = nullptr;

	// Скидає стан, набутий під час гри, щоб наступний запуск створив колеса заново
	void onEditorStop() override;

	wheel mWheelFR = {};
	wheel mWheelFL = {};
	wheel mWheelBR = {};
	wheel mWheelBL = {};

	float mSteering = 0.0f;
	float mSpeed = 0.0f;

	bool isStarted = false;
};

