#pragma once  
#include "Vector2.h"  
#include <Windows.h>

enum MouseButton
{
    MB_Left = 0,
    MB_Right = 1,
    MB_Middle = 2,
    MB_Count
};

static class Input  
{  
public:
   // Перевіряє, чи натиснута клавіша з вказаним кодом
   static bool getKey(int keycode);  
   // Повертає true тільки в той кадр, коли клавішу було натиснуто
   static bool getKeyDown(int keycode);  
   // Повертає true тільки в той кадр, коли клавішу було відпущено
   static bool getKeyUp(int keycode);  

   // Перевіряє, чи натиснута кнопка миші
   static bool getMouseButton(MouseButton button);
   // Повертає true тільки в той кадр, коли кнопку миші було натиснуто
   static bool getMouseButtonDown(MouseButton button);
   // Повертає true тільки в той кадр, коли кнопку миші було відпущено
   static bool getMouseButtonUp(MouseButton button);

   // Повертає true, якщо колесико миші прокручується вгору
   static bool getMouseWheelUp();
   // Повертає true, якщо колесико миші прокручується вниз
   static bool getMouseWheelDown();

   // Повертає зміну позиції миші за кадр
   static Vector2 getDeltaMousePos();

   // Встановлює стан миші
   static void hideCursor(bool hide);
   // Повертає true, якщо курсор прихований
   static bool isCursorHidden();

private:  
   static unsigned char keys_state[256];  
   static unsigned char old_keys_state[256];

   static unsigned char mouse_state[MB_Count];
   static unsigned char old_mouse_state[MB_Count];

   static Vector2 lastTickMousePos;  
   static Vector2 currentTickMousePos;  
   static Vector2 deltaMousePos;  

   static INT16 mouseWheelDelta;
   static INT16 oldMouseWheelDelta;

   static bool mIsCursorHidden;

   // Оновлює стан клавіатури та кнопок миші
   static void update();  
   // Оновлює стан миші з урахуванням області вікна та фокусу
   static void updateMouse(RECT windowRect, bool isFocused);
   // Оновлює стан колесика миші
   static void updateMouseWheel(INT16 delta);

   friend class AppWindow;
   friend class Window;
};
