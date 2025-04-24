#include "Cube.h"

struct vertex {
	Vector3 pos;
	Vector3 col;
};

Cube::Cube()
{
	this->position = Vector3(0.0f, 0.0f, 0.0f);
}

Cube::Cube(Vector3 position)
{
	this->position = position;
}

Cube::~Cube()
{
	mVertexBuffer->release();
	mVertexShader->release();
	mPixelShader->release();
	mIndexBuffer->release();
}

void Cube::init() 
{
	RenderObject::init();

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

	modelM.setTranslation(this->position);
}

void Cube::render()
{
	RenderObject::render();

	GraphicsEngine::engine()->getImmDeviceContext()->setVertexShader(mVertexShader);
	GraphicsEngine::engine()->getImmDeviceContext()->setPixelShader(mPixelShader);

	GraphicsEngine::engine()->getImmDeviceContext()->setVertexBuffer(mVertexBuffer);
	GraphicsEngine::engine()->getImmDeviceContext()->setIndexBuffer(mIndexBuffer);
	GraphicsEngine::engine()->getImmDeviceContext()->drawIndexedTriangleList(mIndexBuffer->getVertexListSize(), 0, 0);
}
