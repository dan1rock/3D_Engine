#include "Input.h"
#include "Windows.h"

unsigned char Input::keys_state[256] = {};
unsigned char Input::old_keys_state[256] = {};

unsigned char Input::mouse_state[MB_Count] = {};
unsigned char Input::old_mouse_state[MB_Count] = {};

Vector2 Input::lastTickMousePos = Vector2(0.0f, 0.0f);
Vector2 Input::currentTickMousePos = Vector2(0.0f, 0.0f);
Vector2 Input::deltaMousePos = Vector2(0.0f, 0.0f);

INT16 Input::mouseWheelDelta = 0;
INT16 Input::oldMouseWheelDelta = 0;

bool Input::mIsCursorHidden = false;

// Перевіряє, чи натиснута клавіша з вказаним кодом
bool Input::getKey(int keycode)
{
	return keys_state[keycode] & 0x80;
}

// Повертає true тільки в той кадр, коли клавішу було натиснуто
bool Input::getKeyDown(int keycode)
{
	return keys_state[keycode] & 0x80 && !(old_keys_state[keycode] & 0x80);
}

// Повертає true тільки в той кадр, коли клавішу було відпущено
bool Input::getKeyUp(int keycode)
{
	return old_keys_state[keycode] & 0x80 && !(keys_state[keycode] & 0x80);
}

// Перевіряє, чи натиснута кнопка миші
bool Input::getMouseButton(MouseButton button)
{
	return mouse_state[button] & 0x80;
}

// Повертає true тільки в той кадр, коли кнопку миші було натиснуто
bool Input::getMouseButtonDown(MouseButton button)
{
	return (mouse_state[button] & 0x80) && !(old_mouse_state[button] & 0x80);
}

// Повертає true тільки в той кадр, коли кнопку миші було відпущено
bool Input::getMouseButtonUp(MouseButton button)
{
	return (old_mouse_state[button] & 0x80) && !(mouse_state[button] & 0x80);
}

bool Input::getMouseWheelUp()
{
	return oldMouseWheelDelta > 0;
}

bool Input::getMouseWheelDown()
{
	return oldMouseWheelDelta < 0;
}

// Повертає зміну позиції миші за кадр
Vector2 Input::getDeltaMousePos()
{
	return deltaMousePos;
}

void Input::hideCursor(bool hide)
{

	if (hide && !mIsCursorHidden)
	{
		::ShowCursor(FALSE);
		mIsCursorHidden = true;
	}

	if (!hide && mIsCursorHidden)
	{
		::ShowCursor(TRUE);
		mIsCursorHidden = false;
	}
}

bool Input::isCursorHidden()
{
	return mIsCursorHidden;
}

// Оновлює стан клавіатури та кнопок миші
void Input::update()
{
	// Оновлюємо стан клавіатури
	::memcpy(old_keys_state, keys_state, sizeof(unsigned char) * 256);
	::GetKeyboardState(keys_state);

	// Оновлюємо стан кнопок миші
	::memcpy(old_mouse_state, mouse_state, sizeof(mouse_state));
	mouse_state[MB_Left] = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 0x80 : 0;
	mouse_state[MB_Right] = (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 0x80 : 0;
	mouse_state[MB_Middle] = (::GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 0x80 : 0;

	// Оновлюємо стан колеса миші
	oldMouseWheelDelta = mouseWheelDelta;
	mouseWheelDelta = 0;
}

bool wasFocused = false;

// Оновлює стан миші з урахуванням області вікна та фокусу
void Input::updateMouse(RECT windowRect, bool isFocused)
{
	// Перевірка, чи вікно має фокус
	if (isFocused)
	{
		POINT currentMousePos = {};
		::GetCursorPos(&currentMousePos);

		// Якщо фокус вікна змінився, оновлюємо позицію миші
		if (wasFocused != isFocused)
		{
			lastTickMousePos = Vector2(currentMousePos.x, currentMousePos.y);
		}

		deltaMousePos = Vector2(
			currentMousePos.x - lastTickMousePos.x,
			currentMousePos.y - lastTickMousePos.y
		);

		// Обмеження позиції миші в межах вікна
		if (mIsCursorHidden)
		{
			::SetCursorPos((windowRect.right - windowRect.left) / 2, (windowRect.bottom - windowRect.top) / 2);
			lastTickMousePos = Vector2((windowRect.right - windowRect.left) / 2, (windowRect.bottom - windowRect.top) / 2);
		}
		else 
		{
			lastTickMousePos = Vector2(currentMousePos.x, currentMousePos.y);
		}
	}
	else deltaMousePos = Vector2(0.0f, 0.0f);

	wasFocused = isFocused;
}

void Input::updateMouseWheel(INT16 delta)
{
	mouseWheelDelta += delta;
}

