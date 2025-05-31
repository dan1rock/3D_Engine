#pragma once
#include "Component.h"

class FrameCounter : public Component
{
public:
	FrameCounter();
	~FrameCounter();

protected:
	FrameCounter* instantiate() const override
	{
		return new FrameCounter(*this);
	}

private:
	void awake() override;
	void update() override;

	int mFrameCount = 0;
	float mTimeAccumulator = 0.0f;
};

