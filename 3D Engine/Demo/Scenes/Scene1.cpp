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
#include "PlaneCollider.h"

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
	Texture* cityTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\city_shadows.png");

	Mesh* sphereMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");
	Mesh* cityMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\city.obj");
	Mesh* cityBaseMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\city_base.obj");

	// Створюємо матеріал для прототипу
	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.0f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 2.0f;
	prototypeMaterial->clampTexture = false;

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

	Material* cityMaterial = new Material();
	Material* cityBaseMaterial = new Material(*cityMaterial);
	cityBaseMaterial->addTexture(cityTexture);

	Entity* cityBase = new Entity(Vector3(0, -10, -10));
	cityBase->getTransform()->setRotation(Vector3(0, 0, 0));
	cityBase->addComponent<MeshRenderer>(cityBaseMesh, cityBaseMaterial);
	cityBase->addComponent<MeshCollider>();
	cityBase->addComponent<RigidBody>(true);

	Entity* city = new Entity();
	city->setParent(cityBase);
	city->addComponent<MeshRenderer>(cityMesh, cityMaterial);
	city->addComponent<MeshCollider>();
	city->addComponent<RigidBody>(true);

	Entity* plane = new Entity(Vector3(0, -1, 0));
	plane->setParent(cityBase);
	plane->getTransform()->setLocalPosition(Vector3(0, 0.02f, 0));
	plane->getTransform()->setLocalRotation(Vector3(0, 0, -3.1416f / 2.0f));
	plane->addComponent<PlaneCollider>();
	plane->addComponent<RigidBody>(true);
}
