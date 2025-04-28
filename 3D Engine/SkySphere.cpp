#include "SkySphere.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"

SkySphere::SkySphere(Matrix* cameraMat)
{
	this->cameraMat = cameraMat;
}

SkySphere::~SkySphere()
{
}

void SkySphere::init()
{
	mTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\sky.jpg");
	mMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");
	mPixelShader = GraphicsEngine::get()->getPixelShader(L"UnlitPixelShader.hlsl", "main");

	RenderObject::init();

	modelM.setScale(Vector3(100.0f, 100.0f, 100.0f));
}

void SkySphere::render()
{
	modelM.setTranslation(cameraMat->getTranslation());

	GraphicsEngine::get()->setRasterizerState(false);

	RenderObject::render();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexShader(mVertexShader);
	GraphicsEngine::get()->getImmDeviceContext()->setPixelShader(mPixelShader);

	GraphicsEngine::get()->getImmDeviceContext()->setTexture(mPixelShader, mTexture);

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);

	GraphicsEngine::get()->setRasterizerState(true);
}
