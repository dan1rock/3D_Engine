#pragma once
#include "RenderComponent.h"

class SkySphere : public RenderComponent
{
public:
	SkySphere();
	~SkySphere() override;

	virtual void render() override;

protected:
	SkySphere* instantiate() const override {
		return new SkySphere(*this);
	};

private:
	void awake() override;
};
