#pragma once
#include <Windows.h>

class Window
{
public:
	Window();
	// Ініціалізує вікно
	bool init();
	// Основний цикл обробки повідомлень та оновлення вікна
	bool broadcast();
	// Звільняє ресурси вікна
	bool release();
	// Перевіряє, чи працює додаток (чи відкрите вікно)
	bool isRunning();

	// Повертає прямокутник клієнтської області вікна
	RECT getClientWindowRect();
	// Повертає прямокутник екрану
	RECT getScreenRect();
	// Встановлює дескриптор вікна
	void setHWND(HWND hwnd);

	// Викликається при створенні вікна (абстрактний метод)
	virtual void onCreate() = 0;
	// Викликається при оновленні вікна (абстрактний метод)
	virtual void onUpdate() = 0;
	// Викликається при зміні розміру вікна (абстрактний метод)
	virtual void onWindowResized() = 0;
	// Викликається при перемиканні повноекранного режиму (абстрактний метод)
	virtual void onFullscreenToggle() = 0;
	// Викликається при прокрутці колеса миші
	virtual void onMouseWheel(INT16 delta) = 0;
	// Викликається при отриманні фокусу вікном
	virtual void onFocus();
	// Викликається при втраті фокусу вікном
	virtual void onKillFocus();
	// Викликається при знищенні вікна
	virtual void onDestroy();

	~Window();
protected:
	HWND mHwnd = NULL;
	bool appIsRunning = false;
};