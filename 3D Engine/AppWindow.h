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
#include "RenderObject.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Matrix.h"
#include <list>

class Texture;
class Mesh;

class AppWindow: public Window
{
public:
	AppWindow();
	void updateDeltaTime();
	void updateDeltaMousePos();
	void updatePosition();
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

	ULONGLONG lastTickTime = 0;
	ULONGLONG currentTickTime = 0;
	float deltaTime = 0;

	Vector2 lastTickMousePos = {};
	Vector2 currentTickMousePos = {};
	Vector2 deltaMousePos = {};

	Vector3 scale = Vector3(1, 1, 1);
	float rotX = 0;
	float rotY = 0;
	float rotZ = 0;

	Matrix worldCam;

	std::list<std::unique_ptr<RenderObject>> renderObjects;

	bool isFocused = false;
};