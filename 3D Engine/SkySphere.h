#pragma once
#include "RenderComponent.h"

class SkySphere : public RenderComponent
{
public:
	SkySphere();
	~SkySphere() override;

	virtual void render() override;

private:
	void awake() override;
};
