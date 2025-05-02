#pragma once  
#include "Vector2.h"  
#include <Windows.h>

class Input  
{  
public:  
   Input();  
   ~Input();  

   static bool getKey(int keycode);  
   static bool getKeyDown(int keycode);  
   static bool getKeyUp(int keycode);  
   static Vector2 getDeltaMousePos();  

private:  
   static unsigned char keys_state[256];  
   static unsigned char old_keys_state[256];  

   static Vector2 lastTickMousePos;  
   static Vector2 currentTickMousePos;  
   static Vector2 deltaMousePos;  

   static void update();  
   static void updateMouse(RECT windowRect, bool isFocused);  

   friend class AppWindow;  
};
