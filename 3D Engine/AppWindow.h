#pragma once
#include "Window.h"
#include "GraphicsEngine.h"
#include "SwapChain.h"
#include "DeviceContext.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "Input.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "GameObject.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Matrix.h"
#include "PhysicsEngine.h"

class AppWindow: public Window
{
public:
	AppWindow();
	~AppWindow();

	// Inherited via Window
	virtual void onCreate() override;
	virtual void onUpdate() override;
	virtual void onWindowResized() override;
	virtual void onFocus() override;
	virtual void onKillFocus() override;
	virtual void onDestroy() override;
private:
	SwapChain* mSwapChain = nullptr;

	bool isFocused = false;
};