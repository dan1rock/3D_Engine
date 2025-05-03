#include "FreelookCameraController.h"
#include "GlobalResources.h"
#include "Input.h"
#include "Time.h"
#include "GameObject.h"

FreelookCameraController::FreelookCameraController()
{
}

FreelookCameraController::FreelookCameraController(float speed, float mouseSpeed)
{
	this->speed = speed;
	this->mouseSpeed = mouseSpeed;
}

FreelookCameraController::~FreelookCameraController()
{
}

void FreelookCameraController::update()
{
	Vector3 rotation = mOwner->getTransform()->getRotation();
	rotation.x += Input::getDeltaMousePos().y * mouseSpeed;
	rotation.y += Input::getDeltaMousePos().x * mouseSpeed;

	Vector3 direction = Vector3(0, 0, 0);

	if (Input::getKey('W'))
	{
		direction.x += 1.0f;
	}
	if (Input::getKey('S'))
	{
		direction.x -= 1.0f;
	}
	if (Input::getKey('A'))
	{
		direction.y -= 1.0f;
	}
	if (Input::getKey('D'))
	{
		direction.y += 1.0f;
	}
	if (Input::getKey(VK_CONTROL))
	{
		direction.z -= 1.0f;
	}
	if (Input::getKey(VK_SPACE))
	{
		direction.z += 1.0f;
	}

	direction.normalize();
	direction *= speed;

	if (Input::getKey(VK_SHIFT))
	{
		direction *= 2.0f;
	}

	mOwner->getTransform()->setRotation(rotation);

	Vector3 newPos = mOwner->getTransform()->getPosition() + mOwner->getTransform()->getMatrix()->getZDirection() * direction.x * Time::getDeltaTime() +
		mOwner->getTransform()->getMatrix()->getXDirection() * direction.y * Time::getDeltaTime() +
		Vector3(0, direction.z * Time::getDeltaTime(), 0);
	mOwner->getTransform()->setPosition(newPos);
}
