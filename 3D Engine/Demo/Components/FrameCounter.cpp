#include "FrameCounter.h"
#include "EngineTime.h"

#include <iostream>

FrameCounter::FrameCounter()
{
}

FrameCounter::~FrameCounter()
{
}

void FrameCounter::awake()
{
}

void FrameCounter::update()
{
	mFrameCount++;
	mTimeAccumulator += Time::getDeltaTime();
	if (mTimeAccumulator >= 1.0f)
	{
		std::cout << "FPS: " << mFrameCount << std::endl;
		mFrameCount = 0;
		mTimeAccumulator = 0.0f;
	}
}
