#pragma once
#include "Windows.h"

class Time
{
public:
	Time();
	~Time();

	// Повертає різницю часу між двома кадрами
	static float getDeltaTime();
	// Повертає час, що пройшов з моменту запуску програми
	static float getCurrentTime();

private:
	// Оновлює поточний час і різницю часу між кадрами
	static void update();

	static float deltaTime;
	static ULONGLONG lastTickTime;
	static ULONGLONG currentTickTime;

	friend class AppWindow;
};

