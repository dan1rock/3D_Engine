#include "DemoPlayer.h"
#include "Input.h"
#include "GameObject.h"
#include "RigidBody.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "TextureManager.h"

DemoPlayer::DemoPlayer(float speed, float sensitivity) : FreelookCameraController(speed, sensitivity)
{
}

DemoPlayer::~DemoPlayer()
{
}

void DemoPlayer::awake()
{
	mMaterial = new Material();
	mMaterial->setPixelShader(GraphicsEngine::get()->getPixelShader(L"PrototypePixelShader.hlsl", "main"));
	mMaterial->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(L"Assets\\Textures\\1x1_grid_quarter_lines_numbered.png"));
	mMaterial->setColor(0.7f, 0.7f, 0.1f, 1.0f);
}

void DemoPlayer::update()
{
	FreelookCameraController::update();

	if (Input::getMouseButtonDown(MB_Left))
	{
		Mesh* mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(L"Assets\\Meshes\\sphere.obj");

		GameObject* newObject = new GameObject(mOwner->getTransform()->getPosition());
		newObject->getTransform()->setForward(mOwner->getTransform()->getForward());
		newObject->getTransform()->setScale(Vector3(0.15f, 0.15f, 0.15f));
		newObject->addComponent<MeshRenderer>(mesh, mMaterial);
		RigidBody* rb = newObject->addComponent<RigidBody>(1.0f, 100.0f);
		rb->setContinousCollisionDetection(true);
		rb->addForce(mOwner->getTransform()->getForward() * 600.0f);
	}
}
