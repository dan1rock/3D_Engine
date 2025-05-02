#include "Input.h"
#include "Windows.h"

unsigned char Input::keys_state[256] = {};
unsigned char Input::old_keys_state[256] = {};

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

Vector2 Input::getDeltaMousePos()
{
	return deltaMousePos;
}

void Input::update()
{
	::memcpy(old_keys_state, keys_state, sizeof(unsigned char) * 256);
	::GetKeyboardState(keys_state);
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

