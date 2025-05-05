#pragma once
#include "Component.h"

class SceneChanger : public Component
{
public:
	SceneChanger();
	~SceneChanger() override;

private:
	void awake() override;
	void update() override;
};

