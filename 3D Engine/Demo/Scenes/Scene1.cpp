#include "Scene1.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "Prefab.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "SkySphere.h"
#include "DemoPlayer.h"
#include "InstantiationTest.h"
#include "Camera.h"
#include "RigidBody.h"
#include "MeshCollider.h"
#include "SphereCollider.h"

Scene1::Scene1()
{
}

Scene1::~Scene1()
{
}

void Scene1::init()
{
	// Отримуємо текстури та меші
	Texture* prototypeTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\1x1_grid_quarter_lines_numbered.png");
	Texture* baseTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\base.png");

	Mesh* sphereMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");
	Mesh* baseMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\base.obj");

	// Створюємо матеріал для прототипу
	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.0f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 2.0f;
	prototypeMaterial->clampTexture = false;

	// Створюємо матеріал для сфери
	Material* sphereMaterial = new Material();
	sphereMaterial->setColor(0.8f, 0.2f, 0.2f, 1.0f);

	// Створюємо сферу
	Entity* sphere = new Entity(Vector3(0.0f, 1.0f, 0.0f));
	sphere->addComponent<MeshRenderer>(sphereMesh, sphereMaterial);

	// Створюємо небесну сферу
	Entity* skyDome = new Entity();
	skyDome->addComponent<SkySphere>();

	Prefab* testPrefab = new Prefab();
	testPrefab->getTransform()->setScale(Vector3(1, 1, 1) * 0.5f);
	testPrefab->addComponent<MeshRenderer>(GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj"), prototypeMaterial);
	testPrefab->addComponent<SphereCollider>();
	testPrefab->addComponent<RigidBody>(1.0f);

	// Створюємо камеру з контролем гравця
	Entity* camera = new Entity(Vector3(0, 1, 3));
	camera->getTransform()->setRotation(Vector3(0, 3.1416f, 0));
	camera->addComponent<Camera>();
	DemoPlayer* demoPlayer = camera->addComponent<DemoPlayer>(2.0f, 0.002f);
	demoPlayer->projectilePrefab = testPrefab;

	Material* baseMaterial = new Material();
	baseMaterial->addTexture(baseTexture);

	Entity* base = new Entity(Vector3(0, -10, -50));
	base->getTransform()->setRotation(Vector3(0, -3.1416f / 2.0f, 0));
	base->addComponent<MeshRenderer>(baseMesh, baseMaterial);
	base->addComponent<MeshCollider>();
	base->addComponent<RigidBody>(true);
}
