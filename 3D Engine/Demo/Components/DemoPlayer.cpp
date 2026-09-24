#include "DemoPlayer.h"
#include "Input.h"
#include "Prefab.h"
#include "RigidBody.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "TextureManager.h"
#include "SceneIO.h"

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
	if (!Input::isCursorHidden()) return;

	FreelookCameraController::update();

	if (Input::getMouseButtonDown(MB_Left) && projectilePrefab)
	{
		Entity* projectile = projectilePrefab->instantiate();
		projectile->getTransform()->setForward(mOwner->getTransform()->getForward());
		projectile->getTransform()->setPosition(mOwner->getTransform()->getPosition() + mOwner->getTransform()->getForward() * 0.5f);
		projectile->getComponent<RigidBody>()->addForce(mOwner->getTransform()->getForward() * 1000.0f);
	}
}

// Записує власні поля та посилання у файл сцени
void DemoPlayer::serialize(SceneWriter& writer) const
{
	writer.writeRef("projectilePrefab", projectilePrefab);
}

// Відновлює власні поля та посилання з файлу сцени
void DemoPlayer::deserialize(const SceneReader& reader)
{
	projectilePrefab = reader.readPrefab("projectilePrefab");
}
