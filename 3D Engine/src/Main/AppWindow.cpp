#include "AppWindow.h"
#include <Windows.h>
#include "GlobalResources.h"
#include "EntityManager.h"
#include "EngineTime.h"
#include "SceneManager.h"
#include "MainScene.h"

#include <iostream>

constant* constantData = nullptr;

AppWindow::AppWindow()
{
}

AppWindow::~AppWindow()
{
}

// Викликається при створенні вікна: ініціалізує рушії, створює SwapChain, встановлює початкові константи та завантажує головну сцену
void AppWindow::onCreate()
{
	// Ініціалізує рушії та створює SwapChain
	PhysicsEngine::get()->init();

	GraphicsEngine::get()->init();
	mSwapChain = GraphicsEngine::get()->createSwapShain();

	// Встановлює початкові константи для глобальних ресурсів
	constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	constantData->world.setIdentity();

	onWindowResized();

	constantData->lightPos[0] = 50.0f;
	constantData->lightPos[1] = 50.0f;
	constantData->lightPos[2] = 50.0f;

	constantData->lightColor[0] = 1.0f;
	constantData->lightColor[1] = 1.0f;
	constantData->lightColor[2] = 1.0f;

	Time::init();

	// Завантажує головну сцену
	SceneManager::get()->loadScene(new MainScene());
}

// Основний цикл оновлення: обробляє ввід, оновлює компоненти, фізику, рендеринг та сцену
void AppWindow::onUpdate()
{
	// Перевіряє, чи вікно має фокус
	if (!isFocused)
	{
		return;
	}

	// Очищає буфер кадру та встановлює розмір вьюпорту
	GraphicsEngine::get()->getImmDeviceContext()->clearRenderTarget(mSwapChain, 0.1f, 0.1f, 0.1f, 1);
	RECT windowSize = this->getClientWindowRect();
	GraphicsEngine::get()->getImmDeviceContext()->setViewportSize(windowSize.right - windowSize.left, windowSize.bottom - windowSize.top);

	if (Input::getKeyDown(VK_ESCAPE))
	{
		appIsRunning = false;
	}

	// Оновлює час та ввід
	Time::update();
	constantData->time = Time::getCurrentTime();

	Input::update();
	Input::updateMouse(this->getClientWindowRect(), isFocused);

	// Виконує фіксоване оновлення фізики та компонентів
	static float accumulator = 0.0f;
	constexpr float fixedStep = 1.0f / 50.0f;
	accumulator += Time::deltaTime;

	if (accumulator >= fixedStep)
	{
		EntityManager::get()->fixedUpdateComponents();
		PhysicsEngine::get()->update(fixedStep);
		accumulator -= fixedStep;
	}

	// Оновлює компоненти
	EntityManager::get()->updateComponents();

	// Оновлює рендер-компоненти та камери
	EntityManager::get()->updateCameras();
	EntityManager::get()->updateRenderers();

	// Оновлює стан менеджера сцен
	SceneManager::get()->update();

	// Виводить кадр на екран
	mSwapChain->present(false);
}

// Викликається при зміні розміру вікна: оновлює SwapChain та матрицю проекції
void AppWindow::onWindowResized()
{
	RECT rc = this->getClientWindowRect();

	mSwapChain->init(this->mHwnd, rc.right - rc.left, rc.bottom - rc.top);

	float aspectRatio = (float)(rc.right - rc.left) / (float)(rc.bottom - rc.top);
	constantData->projection.setPerspectivePM(
		1.1f,
		aspectRatio,
		0.1f,
		200.0f
	);
}

void AppWindow::onMouseWheel(INT16 delta)
{
	if (isFocused)
	{
		Input::updateMouseWheel(delta);
	}
}

// Викликається при отриманні фокусу вікном: ховає курсор та оновлює час
void AppWindow::onFocus()
{
	isFocused = true;
	::ShowCursor(false);
	Time::update();
}

// Викликається при втраті фокусу вікном: показує курсор
void AppWindow::onKillFocus()
{
	isFocused = false;
	::ShowCursor(true);
}

// Викликається при знищенні вікна: звільняє ресурси та завершує роботу рушіїв
void AppWindow::onDestroy()
{
	Window::onDestroy();
	mSwapChain->release();
	PhysicsEngine::get()->shutdown();
	GraphicsEngine::get()->release();
}
