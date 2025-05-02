#include "Time.h"

float Time::deltaTime = 0.0f;
ULONGLONG Time::lastTickTime = 0;
ULONGLONG Time::currentTickTime = 0;

Time::Time()
{
}

Time::~Time()
{
}

float Time::getDeltaTime()
{
    return deltaTime;
}

float Time::getCurrentTime()
{
    return currentTickTime / 1000.0f;
}

void Time::update()
{
	lastTickTime = currentTickTime;
	currentTickTime = ::GetTickCount64();
	deltaTime = (lastTickTime > 0) ? ((currentTickTime - lastTickTime) / 1000.0f) : 0;
}
