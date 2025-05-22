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

	// Викликається при створенні вікна: ініціалізує рушії, створює SwapChain, встановлює початкові константи та завантажує головну сцену
	virtual void onCreate() override;
	// Основний цикл оновлення: обробляє ввід, оновлює компоненти, фізику, рендеринг та сцену
	virtual void onUpdate() override;
	// Викликається при зміні розміру вікна: оновлює SwapChain та матрицю проекції
	virtual void onWindowResized() override;
	// Викликається при отриманні фокусу вікном: ховає курсор та оновлює час
	virtual void onFocus() override;
	// Викликається при втраті фокусу вікном: показує курсор
	virtual void onKillFocus() override;
	// Викликається при знищенні вікна: звільняє ресурси та завершує роботу рушіїв
	virtual void onDestroy() override;
private:
	SwapChain* mSwapChain = nullptr;

	bool isFocused = false;
};