#include "SkySphere.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "GlobalResources.h"
#include "Material.h"
#include "GameObject.h"

SkySphere::SkySphere()
{
}

SkySphere::~SkySphere()
{
}

void SkySphere::init()
{
	mMaterial = new Material();
	mMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"UnlitPixelShader.hlsl", "main"));
	mMaterial->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\HDR_029_Sky_Cloudy_Bg.jpg"));

	mMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

	RenderObject::init();

	mOwner->getTransform()->setScale(Vector3(100.0f, 100.0f, 100.0f));
}

void SkySphere::render()
{
	Vector3 cameraTranslation = GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.getTranslation();
	GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.setTranslation(Vector3());

	GraphicsEngine::get()->setRasterizerState(false);

	RenderObject::render();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);

	GraphicsEngine::get()->setRasterizerState(true);
	GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.setTranslation(cameraTranslation);
}
