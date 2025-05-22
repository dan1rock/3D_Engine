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