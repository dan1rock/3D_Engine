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

constant* constantData = nullptr;

AppWindow::AppWindow()
{
}

void AppWindow::updateDeltaTime()
{
	lastTickTime = currentTickTime;
	currentTickTime = ::GetTickCount64();
	deltaTime = (lastTickTime > 0) ? ((currentTickTime - lastTickTime) / 1000.0f) : 0;
}

void AppWindow::updateDeltaMousePos()
{
	if (isFocused)
	{
		RECT rc = this->getClientWindowRect();
		POINT currentMousePos = {};
		::GetCursorPos(&currentMousePos);
		::SetCursorPos((rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2);
		deltaMousePos = Vector2(
			currentMousePos.x - lastTickMousePos.x,
			currentMousePos.y - lastTickMousePos.y
		);
		lastTickMousePos = Vector2((rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2);
	}
	else deltaMousePos = Vector2(0.0f, 0.0f);
}

void AppWindow::updatePosition()
{
	constantData->time = ::GetTickCount64();

	const float speed = 2.0f;
	const float mouseSpeed = 0.002f;
	const float scaleSpeed = 1.0f;

	if (isFocused)
	{
		rotX += deltaMousePos.y * mouseSpeed;
		rotY += deltaMousePos.x * mouseSpeed;

		if (GetKeyState(VK_LBUTTON) & 0x8000)
		{
			scale = Vector3(
				scale.x + deltaTime * scaleSpeed,
				scale.y + deltaTime * scaleSpeed,
				scale.z + deltaTime * scaleSpeed
			);
		}
		if (GetKeyState(VK_RBUTTON) & 0x8000)
		{
			scale = Vector3(
				scale.x - deltaTime * scaleSpeed,
				scale.y - deltaTime * scaleSpeed,
				scale.z - deltaTime * scaleSpeed
			);
		}
	}

	Vector3 direction = Vector3(0, 0, 0);

	if (Input::getKey('W'))
	{
		direction.x += 1.0f;
	}
	if (Input::getKey('S'))
	{
		direction.x -= 1.0f;
	}
	if (Input::getKey('A'))
	{
		direction.y -= 1.0f;
	}
	if (Input::getKey('D'))
	{
		direction.y += 1.0f;
	}
	if (Input::getKey(VK_CONTROL))
	{
		direction.z -= 1.0f;
	}
	if (Input::getKey(VK_SPACE))
	{
		direction.z += 1.0f;
	}

	direction.normalize();
	direction *= speed;

	if (Input::getKey(VK_SHIFT)) 
	{
		direction *= 2.0f;
	}

	if (Input::getKeyDown(VK_ESCAPE)) 
	{
		appIsRunning = false;
	}

	Matrix temp;
	Matrix cam;

	constantData->world.setIdentity();
	cam.setIdentity();

	temp.setIdentity();
	temp.setRotationX(rotX);
	cam *= temp;

	temp.setIdentity();
	temp.setRotationY(rotY);
	cam *= temp;

	Vector3 newPos = worldCam.getTranslation() + cam.getZDirection() * direction.x * deltaTime +
		cam.getXDirection() * direction.y * deltaTime +
		Vector3(0, direction.z * deltaTime, 0);
	cam.setTranslation(newPos);

	worldCam = cam;

	constantData->cameraPos[0] = worldCam.getTranslation().x;
	constantData->cameraPos[1] = worldCam.getTranslation().y;
	constantData->cameraPos[2] = worldCam.getTranslation().z;

	cam.inverse();
	
	constantData->view = cam;

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}

AppWindow::~AppWindow()
{
}

void AppWindow::onCreate()
{
	GraphicsEngine::get()->init();
	mSwapChain = GraphicsEngine::get()->createSwapShain();

	constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	onWindowResized();

	POINT currentMousePos = {};
	::GetCursorPos(&currentMousePos);
	lastTickMousePos = Vector2(currentMousePos.x, currentMousePos.y);

	worldCam.setTranslation(Vector3(0, 1, 3));
	rotY = 3.1416f;

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

	auto penguin = std::make_unique<MeshRenderer>(Vector3(-1.0f, 0.0f, 0.0f));
	penguin->setMaterial(penguinMaterial);
	penguin->setMesh(penguinMesh);

	renderObjects.push_front(std::move(penguin));

	auto rabbit = std::make_unique<MeshRenderer>(Vector3(1.0f, 0.0f, 0.0f));
	rabbit->setMaterial(rabbitMaterial);
	rabbit->setMesh(rabbitMesh);

	renderObjects.push_front(std::move(rabbit));

	auto skyDome = std::make_unique<SkySphere>();

	renderObjects.push_front(std::move(skyDome));

	GraphicsEngine::get()->setRasterizerState(true);
}

void AppWindow::onUpdate()
{
	GraphicsEngine::get()->getImmDeviceContext()->clearRenderTarget(mSwapChain, 0.1f, 0.1f, 0.1f, 1);
	RECT windowSize = this->getClientWindowRect();
	GraphicsEngine::get()->getImmDeviceContext()->setViewportSize(windowSize.right - windowSize.left, windowSize.bottom - windowSize.top);

	Input::get()->update();

	updateDeltaTime();
	updateDeltaMousePos();
	updatePosition();

	for (auto& renderObject : renderObjects) 
	{
		renderObject->render();
	}

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
