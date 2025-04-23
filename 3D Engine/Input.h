#pragma once
class Input
{
public:
	Input();
	~Input();

	static bool getKey(int keycode);
	static bool getKeyDown(int keycode);
	static bool getKeyUp(int keycode);

	void update();

	static Input* get();

private:
	unsigned char keys_state[256] = {};
	unsigned char old_keys_state[256] = {};
};

