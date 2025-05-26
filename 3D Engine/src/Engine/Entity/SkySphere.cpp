#include "SkySphere.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "GlobalResources.h"
#include "Material.h"
#include "Entity.h"

SkySphere::SkySphere()
{
}

SkySphere::~SkySphere()
{
}

// Викликається при активації компонента: створює матеріал, завантажує текстуру та меш, масштабує об'єкт
void SkySphere::awake()
{
	mMaterial = new Material();
	mMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\UnlitPixelShader.hlsl", "main"));
	mMaterial->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\HDR_029_Sky_Cloudy_Bg.jpg"));
	mMaterial->cullBack = false;

	mMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

	Renderer::awake();

	mOwner->getTransform()->setScale(Vector3(100.0f, 100.0f, 100.0f));
}

// Викликається для рендеру: тимчасово зміщує камеру в центр, рендерить небо, повертає камеру назад
void SkySphere::render()
{
	Vector3 cameraTranslation = GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.getTranslation();
	GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.setTranslation(Vector3());

	Renderer::render();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);

	GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.setTranslation(cameraTranslation);
}
