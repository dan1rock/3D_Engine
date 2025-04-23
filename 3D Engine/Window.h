#pragma once
#include <Windows.h>

class Window
{
public:
	Window();
	bool init();
	bool broadcast();
	bool release();
	bool isRunning();

	RECT getClientWindowRect();
	void setHWND(HWND hwnd);

	virtual void onCreate() = 0;
	virtual void onUpdate() = 0;
	virtual void onFocus();
	virtual void onKillFocus();
	virtual void onDestroy();

	~Window();
protected:
	HWND mHwnd = NULL;
	bool appIsRunning = false;
};