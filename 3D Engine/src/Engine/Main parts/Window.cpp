#include "Window.h"

Window* window = nullptr;

Window::Window()
{
}

bool windowIsResizing = false;

// Головна процедура обробки повідомлень Windows
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg)
	{
	case WM_CREATE:
	{
		// Обробка створення вікна
		window->setHWND(hwnd);
		window->onCreate();
		break;
	}
	case WM_SETFOCUS:
	{
		// Обробка отримання фокусу вікном
		window->onFocus();
		break;
	}
	case WM_KILLFOCUS:
	{
		// Обробка втрати фокусу вікном
		window->onKillFocus();
		break;
	}
	case WM_DESTROY:
	{
		// Обробка знищення вікна
		window->onDestroy();
		::PostQuitMessage(0);
		break;
	}
	case WM_SIZE:
	{
		// Обробка зміни розміру вікна
		if (!windowIsResizing)
		{
			window->onWindowResized();
		}
		break;
	}
	case WM_ENTERSIZEMOVE:
	{
		// Початок зміни розміру або переміщення вікна
		windowIsResizing = true;
		break;
	}
	case WM_EXITSIZEMOVE:
	{
		// Кінець зміни розміру або переміщення вікна
		windowIsResizing = false;
		window->onWindowResized();
		break;
	}
	case WM_MOUSEWHEEL:
	{
		short delta = GET_WHEEL_DELTA_WPARAM(wparam);

		window->onMouseWheel(delta / WHEEL_DELTA);
		break;
	}
	case WM_SYSKEYDOWN:
	{
		// ALT+Enter
		if (wparam == VK_RETURN)
		{
			window->onFullscreenToggle();
			break;
		}
		break;
	}
	default:
		return ::DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return NULL;
}

// Ініціалізує вікно
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

	mHwnd = ::CreateWindowEx(WS_EX_OVERLAPPEDWINDOW, L"WindowClass", L"3D Engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1920, 1080, NULL, NULL, NULL, NULL);
	if (!mHwnd)
		return false;
	::ShowWindow(mHwnd, SW_SHOW);
	::UpdateWindow(mHwnd);

	appIsRunning = true;

	return true;
}

// Основний цикл обробки повідомлень та оновлення вікна
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

// Звільняє ресурси вікна
bool Window::release()
{
	if (!::DestroyWindow(mHwnd))
		return false;

	return true;
}

// Перевіряє, чи працює додаток (чи відкрите вікно)
bool Window::isRunning()
{
	return appIsRunning;
}

// Повертає прямокутник клієнтської області вікна
RECT Window::getClientWindowRect()
{
	RECT rect;
	::GetClientRect(this->mHwnd, &rect);
	return rect;
}

RECT Window::getScreenRect()
{
	RECT rect{};

	rect.right = GetSystemMetrics(SM_CXSCREEN);
	rect.bottom = GetSystemMetrics(SM_CYSCREEN);

	return rect;
}

// Встановлює дескриптор вікна
void Window::setHWND(HWND hwnd)
{
	this->mHwnd = hwnd;
}

// Викликається при отриманні фокусу вікном
void Window::onFocus()
{
}

// Викликається при втраті фокусу вікном
void Window::onKillFocus()
{
}

// Викликається при знищенні вікна
void Window::onDestroy()
{
	appIsRunning = false;
}

Window::~Window()
{
}
