#include "DemoPlayer.h"
#include "Input.h"
#include "Prefab.h"
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
}

void DemoPlayer::update()
{
	FreelookCameraController::update();

	if (Input::getMouseButtonDown(MB_Left) && projectilePrefab)
	{
		GameObject* projectile = projectilePrefab->instantiate();
		projectile->getTransform()->setForward(mOwner->getTransform()->getForward());
		projectile->getTransform()->setPosition(mOwner->getTransform()->getPosition());
		projectile->getComponent<RigidBody>()->addForce(mOwner->getTransform()->getForward() * 1000.0f);
	}
}
