#include "ScenePerformanceTest.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "Prefab.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "SkySphere.h"
#include "Camera.h"
#include "RigidBody.h"
#include "MeshCollider.h"
#include "DemoPlayer.h"
#include "ObjectSpawner.h"
#include "SphereCollider.h"

ScenePerformanceTest::ScenePerformanceTest()
{
}

ScenePerformanceTest::~ScenePerformanceTest()
{
}

void ScenePerformanceTest::init()
{
	Texture* prototypeTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\1x1_grid_quarter_lines_numbered.png");

	Mesh* planeMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\plane.obj");

	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.0f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 100.0f;
	prototypeMaterial->clampTexture = false;

	Entity* plane = new Entity(Vector3(0.0f, -2.0f, 0.0f));
	plane->getTransform()->setScale(Vector3(100.0f, 1.0f, 100.0f));
	plane->addComponent<MeshRenderer>(planeMesh, prototypeMaterial);
	plane->addComponent<MeshCollider>();
	plane->addComponent<RigidBody>(true);

	Entity* camera = new Entity(Vector3(0, 2, 10));
	camera->getTransform()->setRotation(Vector3(0.5f, 3.1416f, 0));
	camera->addComponent<Camera>();
	camera->addComponent<DemoPlayer>(2.0f, 0.002f);

	Entity* skyDome = new Entity();
	skyDome->addComponent<SkySphere>();

	Material* testMaterial = new Material(*prototypeMaterial);
	testMaterial->setColor(0.5f, 0.5f, 0.5f, 1.0f);
	testMaterial->textureScale = 2.0f;

	Prefab* testPrefab = new Prefab();
	testPrefab->getTransform()->setScale(Vector3(1, 1, 1) * 0.5f);
	testPrefab->addComponent<MeshRenderer>(GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj"), testMaterial);
	testPrefab->addComponent<SphereCollider>();
	testPrefab->addComponent<RigidBody>(1.0f);

	Entity* spawner = new Entity();
	spawner->addComponent<ObjectSpawner>(testPrefab);
}
