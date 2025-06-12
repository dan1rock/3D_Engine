#pragma once
#include "Component.h"

class SceneChanger : public Component
{
public:
	SceneChanger();
	~SceneChanger() override;

protected:
	SceneChanger* instantiate() const override {
		return new SceneChanger(*this);
	};

private:
	void awake() override;
	void update() override;

	void drawUI();
};

