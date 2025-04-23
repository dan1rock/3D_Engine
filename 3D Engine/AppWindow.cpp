#include "AppWindow.h"
#include <Windows.h>

struct vertex {
	Vector3 pos;
	Vector3 col;
};

__declspec(align(16))
struct constant {
	Matrix world;
	Matrix view;
	Matrix projection;
	unsigned int time;
};

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
	constant data = {};
	data.time = ::GetTickCount64();

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

	data.world.setIdentity();
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
	
	data.view = cam;

	float aspectRatio = (float)(rc.right - rc.left) / (float)(rc.bottom - rc.top);
	data.projection.setPerspectivePM(
		1.1f,
		aspectRatio,
		0.1f,
		100.0f
	);

	mConstantBuffer->update(GraphicsEngine::engine()->getImmDeviceContext(), &data);
}

AppWindow::~AppWindow()
{
}

void AppWindow::onCreate()
{
	GraphicsEngine::engine()->init();
	mSwapChain = GraphicsEngine::engine()->createSwapShain();

	RECT windowSize = this->getClientWindowRect();
	mSwapChain->init(this->mHwnd, windowSize.right - windowSize.left, windowSize.bottom - windowSize.top);

	POINT currentMousePos = {};
	::GetCursorPos(&currentMousePos);
	lastTickMousePos = Vector2(currentMousePos.x, currentMousePos.y);

	worldCam.setTranslation(Vector3(0, 0, -2));

	vertex vertexList[] = {
		{Vector3(-0.5f, -0.5f, -0.5f)	,	Vector3(0.6f, 0.6f, 0.0f)},// 0
		{Vector3(-0.5f, 0.5f, -0.5f)	,	Vector3(0.7f, 0.7f, 0.0f)},// 1
		{Vector3(0.5f, 0.5f, -0.5f)		,	Vector3(1.0f, 1.0f, 0.0f)},// 2
		{Vector3(0.5f, -0.5f, -0.5f)	,	Vector3(0.8f, 0.8f, 0.0f)},// 3
		
		{Vector3(0.5f, -0.5f, 0.5f)		,	Vector3(0.9f, 0.9f, 0.0f)},// 4
		{Vector3(0.5f, 0.5f, 0.5f)		,	Vector3(1.0f, 1.0f, 0.0f)},// 5
		{Vector3(-0.5f, 0.5f, 0.5f)		,	Vector3(0.8f, 0.8f, 0.0f)},// 6
		{Vector3(-0.5f, -0.5f, 0.5f)	,	Vector3(0.5f, 0.5f, 0.0f)} // 7
	};
	
	unsigned int indexList[] = {
		0,2,1,
		2,0,3,

		4,6,5,
		6,4,7,

		1,5,6,
		5,1,2,

		7,3,0,
		3,7,4,

		3,5,2,
		5,3,4,

		7,1,6,
		1,7,0
	};

	mVertexBuffer = GraphicsEngine::engine()->createVertexBuffer();
	mIndexBuffer = GraphicsEngine::engine()->createIndexBuffer();
	mConstantBuffer = GraphicsEngine::engine()->createConstantBuffer();

	void* shaderByteCode = nullptr;
	SIZE_T shaderSize = 0;

	GraphicsEngine::engine()->compileVertexShader(L"VertexShader.hlsl", "main", &shaderByteCode, &shaderSize);
	mVertexShader = GraphicsEngine::engine()->createVertexShader(shaderByteCode, shaderSize);
	mVertexBuffer->load(vertexList, sizeof(vertex), ARRAYSIZE(vertexList), shaderByteCode, shaderSize);
	mIndexBuffer->load(indexList, ARRAYSIZE(indexList));
	GraphicsEngine::engine()->releaseVertexShader();

	GraphicsEngine::engine()->compilePixelShader(L"PixelShader.hlsl", "main", &shaderByteCode, &shaderSize);
	mPixelShader = GraphicsEngine::engine()->createPixelShader(shaderByteCode, shaderSize);
	GraphicsEngine::engine()->releasePixelShader();

	constant data = {};
	mConstantBuffer->load(&data, sizeof(data));

	ID3D11RasterizerState* rasterState = GraphicsEngine::engine()->createRasterizer();
	GraphicsEngine::engine()->getImmDeviceContext()->setRasterizer(rasterState);
}

void AppWindow::onUpdate()
{
	GraphicsEngine::engine()->getImmDeviceContext()->clearRenderTarget(mSwapChain, 0.1f, 0.1f, 0.1f, 1);
	RECT windowSize = this->getClientWindowRect();
	GraphicsEngine::engine()->getImmDeviceContext()->setViewportSize(windowSize.right - windowSize.left, windowSize.bottom - windowSize.top);

	Input::get()->update();

	updateDeltaTime();
	updateDeltaMousePos();
	updatePosition();

	GraphicsEngine::engine()->getImmDeviceContext()->setConstantBuffer(mVertexShader, mConstantBuffer);
	GraphicsEngine::engine()->getImmDeviceContext()->setConstantBuffer(mPixelShader, mConstantBuffer);

	GraphicsEngine::engine()->getImmDeviceContext()->setVertexShader(mVertexShader);
	GraphicsEngine::engine()->getImmDeviceContext()->setPixelShader(mPixelShader);
	GraphicsEngine::engine()->getImmDeviceContext()->setVertexBuffer(mVertexBuffer);
	GraphicsEngine::engine()->getImmDeviceContext()->setIndexBuffer(mIndexBuffer);
	GraphicsEngine::engine()->getImmDeviceContext()->drawIndexedTriangleList(mIndexBuffer->getVertexListSize(), 0, 0);
	//GraphicsEngine::engine()->getImmDeviceContext()->drawTriangleStrip(mVertexBuffer->getVertexListSize(), 0);
	//GraphicsEngine::engine()->getImmDeviceContext()->drawTriangleList(mVertexBuffer->getVertexListSize(), 0);
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
	mVertexBuffer->release();
	mVertexShader->release();
	mPixelShader->release();
	mConstantBuffer->release();
	mIndexBuffer->release();
	GraphicsEngine::engine()->release();
}
