#include "AppWindow.h"
#include <Windows.h>
#include "GlobalResources.h"
#include "EntityManager.h"
#include "EngineTime.h"
#include "SceneManager.h"
#include "MainScene.h"

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
	PhysicsEngine::get()->init();

	GraphicsEngine::get()->init();
	mSwapChain = GraphicsEngine::get()->createSwapShain();

	constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	constantData->world.setIdentity();

	onWindowResized();

	constantData->lightPos[0] = 50.0f;
	constantData->lightPos[1] = 50.0f;
	constantData->lightPos[2] = 50.0f;

	constantData->lightColor[0] = 1.0f;
	constantData->lightColor[1] = 1.0f;
	constantData->lightColor[2] = 1.0f;

	SceneManager::get()->loadScene(new MainScene());
}

// Основний цикл оновлення: обробляє ввід, оновлює компоненти, фізику, рендеринг та сцену
void AppWindow::onUpdate()
{
	if (!isFocused)
	{
		return;
	}

	GraphicsEngine::get()->getImmDeviceContext()->clearRenderTarget(mSwapChain, 0.1f, 0.1f, 0.1f, 1);
	RECT windowSize = this->getClientWindowRect();
	GraphicsEngine::get()->getImmDeviceContext()->setViewportSize(windowSize.right - windowSize.left, windowSize.bottom - windowSize.top);

	if (Input::getKeyDown(VK_ESCAPE))
	{
		appIsRunning = false;
	}

	Time::update();
	constantData->time = Time::currentTickTime;

	Input::update();
	Input::updateMouse(this->getClientWindowRect(), isFocused);

	EntityManager::get()->updateComponents();

	if (Time::deltaTime > 0.0f)
	{
		EntityManager::get()->fixedUpdateComponents();
		PhysicsEngine::get()->update(Time::deltaTime);
	}

	EntityManager::get()->updateCameras();
	EntityManager::get()->updateRenderers();

	SceneManager::get()->update();

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
		100.0f
	);
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
