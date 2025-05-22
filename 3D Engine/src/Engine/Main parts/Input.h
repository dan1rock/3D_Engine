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

class Input  
{  
public:  
   Input();  
   ~Input();  

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

   // Повертає зміну позиції миші за кадр
   static Vector2 getDeltaMousePos();  

private:  
   static unsigned char keys_state[256];  
   static unsigned char old_keys_state[256];

   static unsigned char mouse_state[MB_Count];
   static unsigned char old_mouse_state[MB_Count];

   static Vector2 lastTickMousePos;  
   static Vector2 currentTickMousePos;  
   static Vector2 deltaMousePos;  

   // Оновлює стан клавіатури та кнопок миші
   static void update();  
   // Оновлює стан миші з урахуванням області вікна та фокусу
   static void updateMouse(RECT windowRect, bool isFocused);  

   friend class AppWindow;  
};
