#include "AppWindow.h"
#include <Windows.h>
#include "Mesh.h"
#include "Cube.h"
#include "MeshRenderer.h"
#include "SkySphere.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "Material.h"
#include "GlobalResources.h"
#include "ComponentManager.h"
#include "Time.h"
#include "Camera.h"
#include "FreelookCameraController.h"

constant* constantData = nullptr;

AppWindow::AppWindow()
{
}

AppWindow::~AppWindow()
{
}

void AppWindow::onCreate()
{
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

	Texture* penguinTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\penguin.png");
	Texture* rabbitTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\256-gradient.png");

	Mesh* penguinMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\penguin.obj");
	Mesh* rabbitMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\rabbit.obj");

	Material* penguinMaterial = new Material();
	penguinMaterial->addTexture(penguinTexture);

	Material* rabbitMaterial = new Material();
	rabbitMaterial->addTexture(rabbitTexture);
	rabbitMaterial->smoothness = 0.1f;

	GameObject* penguin = new GameObject(Vector3(1.0f, 0.0f, 0.0f));
	penguin->addComponent<MeshRenderer>(penguinMesh, penguinMaterial);

	gameObjects.push_front(std::move(penguin));

	GameObject* rabbit = new GameObject(Vector3(-1.0f, 0.0f, 0.0f));
	rabbit->addComponent<MeshRenderer>(rabbitMesh, rabbitMaterial);

	gameObjects.push_front(std::move(rabbit));

	GameObject* skyDome = new GameObject();
	skyDome->addComponent<SkySphere>();

	gameObjects.push_front(std::move(skyDome));

	GameObject* camera = new GameObject(Vector3(0, 1, 3));
	camera->getTransform()->setRotation(Vector3(0, 3.1416f, 0));
	camera->addComponent<Camera>();
	camera->addComponent<FreelookCameraController>(2.0f, 0.002f);

	gameObjects.push_front(std::move(camera));

	GraphicsEngine::get()->setRasterizerState(true);
}

void AppWindow::onUpdate()
{
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

	GraphicsEngine::get()->getComponentManager()->updateComponents();
	GraphicsEngine::get()->getComponentManager()->updateCameras();
	GraphicsEngine::get()->getComponentManager()->updateRenderers();

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
	GraphicsEngine::get()->release();
}
