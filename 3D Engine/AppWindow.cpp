#include "AppWindow.h"
#include <Windows.h>
#include "GlobalResources.h"
#include "ComponentManager.h"
#include "Time.h"
#include "SceneManager.h"
#include "MainScene.h"

constant* constantData = nullptr;

AppWindow::AppWindow()
{
}

AppWindow::~AppWindow()
{
}

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

	ComponentManager::get()->updateComponents();

	if (Time::deltaTime >= 0.0f)
	{
		ComponentManager::get()->fixedUpdateComponents();
		PhysicsEngine::get()->update(Time::deltaTime);
	}

	ComponentManager::get()->updateCameras();
	ComponentManager::get()->updateRenderers();

	SceneManager::get()->update();

	mSwapChain->present(false);
}

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

void AppWindow::onFocus()
{
	isFocused = true;
	::ShowCursor(false);
}

void AppWindow::onKillFocus()
{
	isFocused = false;
	::ShowCursor(true);
}

void AppWindow::onDestroy()
{
	Window::onDestroy();
	mSwapChain->release();
	PhysicsEngine::get()->shutdown();
	GraphicsEngine::get()->release();
}
