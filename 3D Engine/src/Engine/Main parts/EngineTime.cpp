#include "EngineTime.h"

float Time::deltaTime = 0.0f;
ULONGLONG Time::lastTickTime = 0;
ULONGLONG Time::currentTickTime = 0;

Time::Time()
{
}

Time::~Time()
{
}

// Повертає різницю часу між двома кадрами
float Time::getDeltaTime()
{
    return deltaTime;
}

// Повертає час, що пройшов з моменту запуску програми
float Time::getCurrentTime()
{
    return currentTickTime / 1000.0f;
}

// Оновлює поточний час і різницю часу між кадрами
void Time::update()
{
	lastTickTime = currentTickTime;
	currentTickTime = ::GetTickCount64();
	deltaTime = (lastTickTime > 0) ? ((currentTickTime - lastTickTime) / 1000.0f) : 0;
}
