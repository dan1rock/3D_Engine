#include "Input.h"
#include "Windows.h"

Input::Input()
{
}

Input::~Input()
{
}

bool Input::getKey(int keycode)
{
	return get()->keys_state[keycode] & 0x80;
}

bool Input::getKeyDown(int keycode)
{
	return get()->keys_state[keycode] & 0x80 && !(get()->old_keys_state[keycode] & 0x80);
}

bool Input::getKeyUp(int keycode)
{
	return get()->old_keys_state[keycode] & 0x80 && !(get()->keys_state[keycode] & 0x80);
}

void Input::update()
{
	::memcpy(old_keys_state, keys_state, sizeof(unsigned char) * 256);
	::GetKeyboardState(keys_state);
}

Input* Input::get()
{
	static Input input;
	return &input;
}
