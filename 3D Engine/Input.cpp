#include "Input.h"
#include "Windows.h"

unsigned char Input::keys_state[256] = {};
unsigned char Input::old_keys_state[256] = {};

unsigned char Input::mouse_state[MB_Count] = {};
unsigned char Input::old_mouse_state[MB_Count] = {};

Vector2 Input::lastTickMousePos = Vector2(0.0f, 0.0f);
Vector2 Input::currentTickMousePos = Vector2(0.0f, 0.0f);
Vector2 Input::deltaMousePos = Vector2(0.0f, 0.0f);

Input::Input()
{
}

Input::~Input()
{
}

bool Input::getKey(int keycode)
{
	return keys_state[keycode] & 0x80;
}

bool Input::getKeyDown(int keycode)
{
	return keys_state[keycode] & 0x80 && !(old_keys_state[keycode] & 0x80);
}

bool Input::getKeyUp(int keycode)
{
	return old_keys_state[keycode] & 0x80 && !(keys_state[keycode] & 0x80);
}

bool Input::getMouseButton(MouseButton button)
{
	return mouse_state[button] & 0x80;
}

bool Input::getMouseButtonDown(MouseButton button)
{
	return (mouse_state[button] & 0x80) && !(old_mouse_state[button] & 0x80);
}

bool Input::getMouseButtonUp(MouseButton button)
{
	return (old_mouse_state[button] & 0x80) && !(mouse_state[button] & 0x80);
}

Vector2 Input::getDeltaMousePos()
{
	return deltaMousePos;
}

void Input::update()
{
	::memcpy(old_keys_state, keys_state, sizeof(unsigned char) * 256);
	::GetKeyboardState(keys_state);

	::memcpy(old_mouse_state, mouse_state, sizeof(mouse_state));
	mouse_state[MB_Left] = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) ? 0x80 : 0;
	mouse_state[MB_Right] = (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) ? 0x80 : 0;
	mouse_state[MB_Middle] = (::GetAsyncKeyState(VK_MBUTTON) & 0x8000) ? 0x80 : 0;
}

bool wasFocused = false;

void Input::updateMouse(RECT windowRect, bool isFocused)
{
	if (isFocused)
	{
		POINT currentMousePos = {};
		::GetCursorPos(&currentMousePos);

		if (wasFocused != isFocused)
		{
			lastTickMousePos = Vector2(currentMousePos.x, currentMousePos.y);
		}

		::SetCursorPos((windowRect.right - windowRect.left) / 2, (windowRect.bottom - windowRect.top) / 2);
		deltaMousePos = Vector2(
			currentMousePos.x - lastTickMousePos.x,
			currentMousePos.y - lastTickMousePos.y
		);

		lastTickMousePos = Vector2((windowRect.right - windowRect.left) / 2, (windowRect.bottom - windowRect.top) / 2);
	}
	else deltaMousePos = Vector2(0.0f, 0.0f);

	wasFocused = isFocused;
}

