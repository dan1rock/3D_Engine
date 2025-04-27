#include "AppWindow.h"
#include <Windows.h>
#include "Mesh.h"

__declspec(align(16))
struct constant {
	Matrix world;
	Matrix model;
	Matrix view;
	Matrix projection;
	unsigned int time;
};

constant constantData = { };

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
	constantData.time = ::GetTickCount64();

	RECT rc = this->getClientWindowRect();

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

	constantData.world.setIdentity();
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

	cam.inverse();
	
	constantData.view = cam;

	float aspectRatio = (float)(rc.right - rc.left) / (float)(rc.bottom - rc.top);
	constantData.projection.setPerspectivePM(
		1.1f,
		aspectRatio,
		0.1f,
		100.0f
	);

	mConstantBuffer->update(GraphicsEngine::get()->getImmDeviceContext(), &constantData);
}

AppWindow::~AppWindow()
{
}

void AppWindow::onCreate()
{
	GraphicsEngine::get()->init();
	mSwapChain = GraphicsEngine::get()->createSwapShain();

	RECT windowSize = this->getClientWindowRect();
	mSwapChain->init(this->mHwnd, windowSize.right - windowSize.left, windowSize.bottom - windowSize.top);

	POINT currentMousePos = {};
	::GetCursorPos(&currentMousePos);
	lastTickMousePos = Vector2(currentMousePos.x, currentMousePos.y);

	worldCam.setTranslation(Vector3(0, 0, -2));

	Texture* texture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\cobblestone.jpg");

	Texture* penguinTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\penguin.png");
	Mesh* penguinMesh = new Mesh(L"Assets\\Meshes\\penguin.obj");

	resources.emplace_front(texture);
	resources.emplace_front(penguinTexture);
	resources.emplace_front(penguinMesh);

	auto cube1 = std::make_unique<Cube>();
	cube1->setTexture(texture);

	auto cube2 = std::make_unique<Cube>(Vector3(2.0f, 0.0f, 0.0f));
	cube2->setTexture(texture);

	renderObjects.push_front(std::move(cube1));
	renderObjects.push_front(std::move(cube2));

	auto penguin = std::make_unique<MeshRenderer>(Vector3(-2.0f, 0.0f, 0.0f));
	penguin->setTexture(penguinTexture);
	penguin->setMesh(penguinMesh);

	renderObjects.push_front(std::move(penguin));

	mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();

	constant data = {};
	mConstantBuffer->load(&data, sizeof(data));

	ID3D11RasterizerState* rasterState = GraphicsEngine::get()->createRasterizer();
	GraphicsEngine::get()->getImmDeviceContext()->setRasterizer(rasterState);
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
		constantData.model = *renderObject->getModelMatrix();
		mConstantBuffer->update(GraphicsEngine::get()->getImmDeviceContext(), &constantData);
		renderObject->setConstantBuffer(mConstantBuffer);
		renderObject->render();
	}

	mSwapChain->present(false);
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
	mConstantBuffer->release();
	GraphicsEngine::get()->release();
}
