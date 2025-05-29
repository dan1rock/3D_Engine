#pragma once
#include "Windows.h"

static class Time
{
public:
	// Повертає різницю часу між двома кадрами
	static double getDeltaTime();
	// Повертає час, що пройшов з моменту запуску програми
	static double getCurrentTime();

private:
	// Ініціалізує таймер, викликається при запуску програми
	static void init();
	// Оновлює поточний час і різницю часу між кадрами
	static void update();

	static double deltaTime;
	static double currentTickTime;
	static LARGE_INTEGER s_frequency;
	static LARGE_INTEGER s_startCount;
	static LARGE_INTEGER s_lastCount;

	friend class AppWindow;
};

