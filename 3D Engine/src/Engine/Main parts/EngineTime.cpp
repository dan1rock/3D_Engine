#include "EngineTime.h"

LARGE_INTEGER Time::s_frequency = {};
LARGE_INTEGER Time::s_startCount = {};
LARGE_INTEGER Time::s_lastCount = {};
double Time::deltaTime = 0.0f;
double Time::currentTickTime = 0.0f;

// Повертає різницю часу між двома кадрами
double Time::getDeltaTime()
{
    return deltaTime;
}

// Повертає час, що пройшов з моменту запуску програми
double Time::getCurrentTime()
{
    return currentTickTime;
}

// Ініціалізує таймер, викликається при запуску програми
void Time::init()
{
	QueryPerformanceFrequency(&s_frequency);
	QueryPerformanceCounter(&s_startCount);
	s_lastCount = s_startCount;
}

// Оновлює поточний час і різницю часу між кадрами
void Time::update()
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    auto counts = now.QuadPart - s_lastCount.QuadPart;
    deltaTime = static_cast<double>(counts) / static_cast<double>(s_frequency.QuadPart);
	currentTickTime = static_cast<double>(now.QuadPart - s_startCount.QuadPart) / static_cast<double>(s_frequency.QuadPart);

    s_lastCount = now;
}
