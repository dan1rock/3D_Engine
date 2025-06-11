#include "AppWindow.h"


// Точка входу в програму
int main() {
	AppWindow app;
	if (app.init()) {
		while (app.isRunning()) {
			app.broadcast();
		}
	}

	return 0;
}

// Точка входу в програму для Windows
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{ 
	AppWindow app;
	if (app.init()) {
		while (app.isRunning()) {
			app.broadcast();
		}
	}

	return 0;
}