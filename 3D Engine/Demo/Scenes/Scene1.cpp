#include "Scene1.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "GameObject.h"
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
	Texture* prototypeTexture = GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\1x1_grid_quarter_lines_numbered.png");

	Mesh* planeMesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\plane.obj");

	Material* prototypeMaterial = new Material();
	prototypeMaterial->addTexture(prototypeTexture);
	prototypeMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PrototypePixelShader.hlsl", "main"));
	prototypeMaterial->setColor(0.5f, 0.5f, 0.5f, 1.0f);
	prototypeMaterial->textureScale = 20.0f;
	prototypeMaterial->clampTexture = false;

	GameObject* plane = new GameObject(Vector3(0.0f, -2.0f, 0.0f));
	plane->getTransform()->setScale(Vector3(20.0f, 1.0f, 20.0f));
	plane->addComponent<MeshRenderer>(planeMesh, prototypeMaterial);
	plane->addComponent<MeshCollider>();
	plane->addComponent<RigidBody>(true);

	GameObject* skyDome = new GameObject();
	skyDome->addComponent<SkySphere>();

	GameObject* camera = new GameObject(Vector3(0, 1, 3));
	camera->getTransform()->setRotation(Vector3(0, 3.1416f, 0));
	camera->addComponent<Camera>();
	camera->addComponent<DemoPlayer>(2.0f, 0.002f);

	GameObject* test = new GameObject(Vector3(0, 0, 0));
	test->addComponent<InstantiationTest>();
}
