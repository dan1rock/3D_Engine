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

   static bool getKey(int keycode);  
   static bool getKeyDown(int keycode);  
   static bool getKeyUp(int keycode);  

   static bool getMouseButton(MouseButton button);
   static bool getMouseButtonDown(MouseButton button);
   static bool getMouseButtonUp(MouseButton button);

   static Vector2 getDeltaMousePos();  

private:  
   static unsigned char keys_state[256];  
   static unsigned char old_keys_state[256];

   static unsigned char mouse_state[MB_Count];
   static unsigned char old_mouse_state[MB_Count];

   static Vector2 lastTickMousePos;  
   static Vector2 currentTickMousePos;  
   static Vector2 deltaMousePos;  

   static void update();  
   static void updateMouse(RECT windowRect, bool isFocused);  

   friend class AppWindow;  
};
