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
	// Знищене небо більше не освітлює сцену
	if (mSkyTexture && GraphicsEngine::get()->getEnvironmentMap() == mSkyTexture) GraphicsEngine::get()->setEnvironmentMap(nullptr);
}

// Викликається при активації компонента: створює матеріал, завантажує текстуру та меш, масштабує об'єкт
void SkySphere::awake()
{
	Material* material = new Material();
	material->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\UnlitPixelShader.hlsl", "main"));

	mSkyTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\HDR_029_Sky_Cloudy_Bg.jpg");

	material->setMap(MaterialMap::Base, mSkyTexture);

	// Камера всередині сфери, тож видно її внутрішній бік
	material->properties.renderFace = RenderFace::Back;

	// Небо освітлює сцену й відбивається в ній
	GraphicsEngine::get()->setEnvironmentMap(mSkyTexture);

	setMaterial(material);

	mMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

	// Небо оточує всю сцену, тому не повинно потрапляти в карту тіней
	castShadows = false;

	// Небо малюється зміщеним у центр камери, тому його світові межі не описують того, що видно
	alwaysVisible = true;

	Renderer::awake();

	mOwner->getTransform()->setScale(Vector3(200.0f, 200.0f, 200.0f));
}

// Викликається для рендеру: тимчасово зміщує камеру в центр, рендерить небо, повертає камеру назад
void SkySphere::render()
{
	// Небо непрозоре, тож у проході прозорих його немає
	if (GraphicsEngine::get()->getRenderPass() != RenderPass::Opaque) return;

	Vector3 cameraTranslation = GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.getTranslation();
	GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.setTranslation(Vector3());

	Renderer::render();

	applyMaterial(0);

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);

	GraphicsEngine::get()->getGlobalResources()->getConstantData()->view.setTranslation(cameraTranslation);
}
