#pragma once
#include "Windows.h"

class Time
{
public:
	Time();
	~Time();
	static float getDeltaTime();
	static float getCurrentTime();

private:
	static void update();

	static float deltaTime;
	static ULONGLONG lastTickTime;
	static ULONGLONG currentTickTime;

	friend class AppWindow;
};

