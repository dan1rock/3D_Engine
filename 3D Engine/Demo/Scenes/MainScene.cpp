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
#include "CarComponent.h"
#include "FollowComponent.h"
#include "FrameCounter.h"

#include <iostream>

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
	Texture* carTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\car.png");
	Texture* wheelTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\wheel.png");

	Mesh* penguinMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\penguin.obj");
	Mesh* rabbitMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\rabbit.obj");
	Mesh* planeMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\plane.obj");
	Mesh* offroadMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\offroad.obj");
	Mesh* carMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\car.obj");
	Mesh* wheelMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\wheel.obj");

	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.5f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 40.0f;
	prototypeMaterial->clampTexture = false;

	Material* penguinMaterial = new Material();
	penguinMaterial->addTexture(penguinTexture);

	Material* rabbitMaterial = new Material();
	rabbitMaterial->addTexture(rabbitTexture);
	rabbitMaterial->smoothness = 0.1f;

	Material* carMaterial = new Material();
	carMaterial->addTexture(carTexture);

	Material* wheelMaterial = new Material();
	wheelMaterial->addTexture(wheelTexture);

	Entity* offroad = new Entity(Vector3(100.0f, -2.0f, 0.0f));
	offroad->getTransform()->setScale(Vector3(40.0f, 1.0f, 40.0f));
	offroad->getTransform()->setScale(Vector3(2.0f, 0.3f, 2.0f));
	offroad->addComponent<MeshRenderer>(offroadMesh);
	offroad->addComponent<MeshCollider>();
	offroad->addComponent<RigidBody>(true);

	Entity* plane = new Entity(Vector3(0.0f, -2.01f, 0.0f));
	plane->getTransform()->setScale(Vector3(80.0f, 1.0f, 80.0f));
	plane->addComponent<MeshRenderer>(planeMesh, prototypeMaterial);
	plane->addComponent<MeshCollider>();
	plane->addComponent<RigidBody>(true);

	/*Entity* penguin = new Entity(Vector3(1.0f, 0.0f, 0.0f));
	penguin->addComponent<MeshRenderer>(penguinMesh, penguinMaterial);

	Entity* rabbit = new Entity(Vector3(-1.0f, 0.0f, 0.0f));
	rabbit->addComponent<MeshRenderer>(rabbitMesh, rabbitMaterial);*/

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
	FollowComponent* followComponent = camera->addComponent<FollowComponent>();
	followComponent->setSpeed(5.0f);

	Prefab* wheelPrefab = new Prefab();
	wheelPrefab->addComponent<MeshRenderer>(wheelMesh, wheelMaterial);

	Prefab* carPrefab = new Prefab();
	carPrefab->addComponent<MeshRenderer>(carMesh, carMaterial);
	carPrefab->addComponent<MeshCollider>();
	carPrefab->addComponent<RigidBody>(1.0f);
	CarComponent* c = carPrefab->addComponent<CarComponent>();
	c->force = 30.0f;
	c->damping = 10.0f;
	c->maxDistance = 0.15f;
	c->wheelPrefab = wheelPrefab;

	Entity* test = new Entity(Vector3(0, 0, 0));
	InstantiationTest* i = test->addComponent<InstantiationTest>();
	i->prefab = carPrefab;

	Entity* sceneChanger = new Entity();
	sceneChanger->addComponent<FrameCounter>();
	sceneChanger->addComponent<SceneChanger>();

	Entity* playerCar = carPrefab->instantiate();

	followComponent->setTarget(playerCar->getTransform());
}
