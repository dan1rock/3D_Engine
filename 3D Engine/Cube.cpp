#include "Cube.h"

struct vertex {
	Vector3 pos;
	Vector2 texCoord;
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
		{Vector3(-0.5f, -0.5f, -0.5f)	,	Vector2(1, 0)},// 0
		{Vector3(-0.5f, 0.5f, -0.5f)	,	Vector2(0, 0)},// 1
		{Vector3(0.5f, 0.5f, -0.5f)		,	Vector2(0, 1)},// 2
		{Vector3(0.5f, -0.5f, -0.5f)	,	Vector2(1, 1)},// 3

		{Vector3(0.5f, -0.5f, 0.5f)		,	Vector2(1, 0)},// 4
		{Vector3(0.5f, 0.5f, 0.5f)		,	Vector2(0, 0)},// 5
		{Vector3(-0.5f, 0.5f, 0.5f)		,	Vector2(0, 1)},// 6
		{Vector3(-0.5f, -0.5f, 0.5f)	,	Vector2(1, 1)} // 7
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

	mVertexBuffer = GraphicsEngine::get()->createVertexBuffer();
	mIndexBuffer = GraphicsEngine::get()->createIndexBuffer();

	void* shaderByteCode = nullptr;
	SIZE_T shaderSize = 0;

	GraphicsEngine::get()->compileVertexShader(L"VertexShader.hlsl", "main", &shaderByteCode, &shaderSize);
	mVertexShader = GraphicsEngine::get()->createVertexShader(shaderByteCode, shaderSize);
	mVertexBuffer->load(vertexList, sizeof(vertex), ARRAYSIZE(vertexList), shaderByteCode, shaderSize);
	mIndexBuffer->load(indexList, ARRAYSIZE(indexList));
	GraphicsEngine::get()->releaseVertexShader();

	GraphicsEngine::get()->compilePixelShader(L"PixelShader.hlsl", "main", &shaderByteCode, &shaderSize);
	mPixelShader = GraphicsEngine::get()->createPixelShader(shaderByteCode, shaderSize);
	GraphicsEngine::get()->releasePixelShader();

	modelM.setTranslation(this->position);
}

void Cube::render()
{
	RenderObject::render();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexShader(mVertexShader);
	GraphicsEngine::get()->getImmDeviceContext()->setPixelShader(mPixelShader);

	GraphicsEngine::get()->getImmDeviceContext()->setTexture(mPixelShader, mTexture);

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mVertexBuffer);
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mIndexBuffer);
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mIndexBuffer->getVertexListSize(), 0, 0);
}
