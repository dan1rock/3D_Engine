#pragma once
#include "Component.h"

class Camera : public Component
{
public:
	Camera();
	~Camera() override;

protected:
	Camera* instantiate() const override {
		return new Camera(*this);
	};

	void registerComponent() override;

private:
	void updateCamera();
	void awake() override;

	friend class ComponentManager;
};

