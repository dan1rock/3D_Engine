#pragma once
#include "Component.h"

class Transform;

class FollowComponent : public Component
{
public:
	COMPONENT_TYPE(FollowComponent)

	FollowComponent();
	~FollowComponent() override;

	void setTarget(Transform* target);
	void setOffset(float offset);
	void setSpeed(float speed);
	void setMouseSpeed(float mouseSpeed);

	// Записує власні поля та посилання у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює власні поля та посилання з файлу сцени
	void deserialize(const SceneReader& reader) override;

protected:
	FollowComponent* instantiate() const override
	{
		return new FollowComponent(*this);
	}

private:
	void update() override;

	Transform* mTarget = nullptr; 
	float mOffset = 5.0f;
	float mSpeed = 1.0f;
	float mMouseSpeed = 0.005f;
	static constexpr float PITCH_LIMIT = 89.0f * (3.14159265f / 180.0f);

	float mYaw = 0.0f;
	float mPitch = 0.5f;
};

