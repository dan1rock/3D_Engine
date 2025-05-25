#include "MainScene.h"
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
#include "SceneChanger.h"
#include "MeshCollider.h"
#include "SphereCollider.h"
#include "LevitationTest.h"

MainScene::MainScene()
{
}

MainScene::~MainScene()
{
}

void MainScene::init()
{
	Texture* penguinTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\penguin.png");
	Texture* rabbitTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\256-gradient.png");
	Texture* prototypeTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\1x1_grid_quarter_lines_numbered.png");

	Mesh* penguinMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\penguin.obj");
	Mesh* rabbitMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\rabbit.obj");
	Mesh* planeMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\plane.obj");

	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.5f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 20.0f;
	prototypeMaterial->clampTexture = false;

	Material* penguinMaterial = new Material();
	penguinMaterial->addTexture(penguinTexture);

	Material* rabbitMaterial = new Material();
	rabbitMaterial->addTexture(rabbitTexture);
	rabbitMaterial->smoothness = 0.1f;

	Entity* plane = new Entity(Vector3(0.0f, -2.0f, 0.0f));
	plane->getTransform()->setScale(Vector3(20.0f, 1.0f, 20.0f));
	plane->addComponent<MeshRenderer>(planeMesh, prototypeMaterial);
	plane->addComponent<MeshCollider>();
	plane->addComponent<RigidBody>(true);

	Entity* penguin = new Entity(Vector3(1.0f, 0.0f, 0.0f));
	penguin->addComponent<MeshRenderer>(penguinMesh, penguinMaterial);

	Entity* rabbit = new Entity(Vector3(-1.0f, 0.0f, 0.0f));
	rabbit->addComponent<MeshRenderer>(rabbitMesh, rabbitMaterial);

	Entity* skyDome = new Entity();
	skyDome->addComponent<SkySphere>();

	Mesh* mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

	Material* projectileMaterial = new Material();
	projectileMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	projectileMaterial->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\1x1_grid_quarter_lines_numbered.png"));
	projectileMaterial->setColor(0.7f, 0.7f, 0.1f, 1.0f);

	Prefab* projectilePrefab = new Prefab();
	projectilePrefab->getTransform()->setScale(Vector3(0.15f, 0.15f, 0.15f));
	projectilePrefab->addComponent<MeshRenderer>(mesh, projectileMaterial);
	projectilePrefab->addComponent<SphereCollider>();
	RigidBody* rb = projectilePrefab->addComponent<RigidBody>(1.0f);
	rb->setContinousCollisionDetection(true);

	Entity* camera = new Entity(Vector3(0, 1, 3));
	camera->getTransform()->setRotation(Vector3(0, 3.1416f, 0));
	camera->addComponent<Camera>();
	DemoPlayer* demoPlayer = camera->addComponent<DemoPlayer>(2.0f, 0.002f);
	demoPlayer->projectilePrefab = projectilePrefab;

	Prefab* instantiationPrefab = new Prefab();
	instantiationPrefab->addComponent<MeshRenderer>(rabbitMesh);
	instantiationPrefab->addComponent<MeshCollider>();
	instantiationPrefab->addComponent<RigidBody>(1.0f);
	LevitationTest* l = instantiationPrefab->addComponent<LevitationTest>();
	l->force = 15.0f;
	l->damping = 2.0f;
	l->maxDistance = 1.0f;

	Entity* test = new Entity(Vector3(0, 0, 0));
	InstantiationTest* i = test->addComponent<InstantiationTest>();
	i->prefab = instantiationPrefab;

	Entity* sceneChanger = new Entity();
	sceneChanger->addComponent<SceneChanger>();
	sceneChanger->dontDestroyOnLoad = true;
}
