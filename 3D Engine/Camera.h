#pragma once
#include "Component.h"

class Camera : public Component
{
public:
	Camera();
	~Camera();

private:
	void updateCamera();

	friend class ComponentManager;
};

