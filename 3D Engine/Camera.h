#pragma once
#include "Component.h"

class Camera : public Component
{
public:
	Camera();
	~Camera() override;

private:
	void updateCamera();

	friend class ComponentManager;
};

