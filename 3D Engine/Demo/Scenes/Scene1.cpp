#include "Scene1.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "Entity.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "SkySphere.h"
#include "DemoPlayer.h"
#include "InstantiationTest.h"
#include "Camera.h"
#include "RigidBody.h"
#include "MeshCollider.h"

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

	Mesh* planeMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\plane.obj");
	Mesh* sphereMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

	// Створюємо матеріал для прототипу
	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.0f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 20.0f;
	prototypeMaterial->clampTexture = false;

	// Створюємо площину з фізичним тілом та колайдером
	Entity* plane = new Entity(Vector3(0.0f, -2.0f, 0.0f));
	plane->getTransform()->setScale(Vector3(20.0f, 1.0f, 20.0f));
	plane->addComponent<MeshRenderer>(planeMesh, prototypeMaterial);
	plane->addComponent<MeshCollider>();
	plane->addComponent<RigidBody>(true);

	// Створюємо матеріал для сфери
	Material* sphereMaterial = new Material();
	sphereMaterial->setColor(0.8f, 0.2f, 0.2f, 1.0f);

	// Створюємо сферу
	Entity* sphere = new Entity(Vector3(0.0f, 1.0f, 0.0f));
	sphere->addComponent<MeshRenderer>(sphereMesh, sphereMaterial);

	// Створюємо небесну сферу
	Entity* skyDome = new Entity();
	skyDome->addComponent<SkySphere>();

	// Створюємо камеру з контролем гравця
	Entity* camera = new Entity(Vector3(0, 1, 3));
	camera->getTransform()->setRotation(Vector3(0, 3.1416f, 0));
	camera->addComponent<Camera>();
	camera->addComponent<DemoPlayer>(2.0f, 0.002f);
}
