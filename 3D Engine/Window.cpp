#include "Window.h"

Window* window = nullptr;

Window::Window()
{
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) 
	{
	case WM_CREATE: 
	{
		window->setHWND(hwnd);
		window->onCreate();
		break;
	}
	case WM_SETFOCUS:
	{
		window->onFocus();
		break;
	}
	case WM_KILLFOCUS:
	{
		window->onKillFocus();
		break;
	}
	case WM_DESTROY:
	{
		window->onDestroy();
		::PostQuitMessage(0);
		break;
	}
	default:
		return ::DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return NULL;
}

bool Window::init()
{
	WNDCLASSEX wc;
	wc.cbClsExtra = NULL;
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.cbWndExtra = NULL;
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = NULL;
	wc.lpszClassName = L"WindowClass";
	wc.lpszMenuName = L"";
	wc.style = NULL;
	wc.lpfnWndProc = &WndProc;

	if (!window)
		window = this;

	if (!::RegisterClassEx(&wc))
		return false;

	mHwnd = ::CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, L"WindowClass", L"3D Engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL, NULL, NULL, NULL);
	if (!mHwnd)
		return false;
	::ShowWindow(mHwnd, SW_SHOW);
	::UpdateWindow(mHwnd);

	appIsRunning = true;

	return true;
}

bool Window::broadcast()
{
	MSG msg;

	this->onUpdate();

	while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	Sleep(0);

	return true;
}

bool Window::release()
{
	if (!::DestroyWindow(mHwnd))
		return false;

	return true;
}

bool Window::isRunning()
{
	return appIsRunning;
}

RECT Window::getClientWindowRect()
{
	RECT rect;
	::GetClientRect(this->mHwnd, &rect);
	return rect;
}

void Window::setHWND(HWND hwnd)
{
	this->mHwnd = hwnd;
}

void Window::onFocus()
{
}

void Window::onKillFocus()
{
}

void Window::onDestroy()
{
	appIsRunning = false;
}

Window::~Window()
{
}
