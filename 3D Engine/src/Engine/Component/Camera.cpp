#include "Camera.h"
#include "GameObject.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "EntityManager.h"

Camera::Camera()
{
}

// Деструктор класу Camera, знімає реєстрацію камери в EntityManager
Camera::~Camera()
{
	EntityManager::get()->unregisterCamera(this);
}

// Реєструє компонент як звичайний компонент і як камеру в EntityManager
void Camera::registerComponent()
{
	Component::registerComponent();
	EntityManager::get()->registerCamera(this);
}

// Оновлює дані камери у глобальних константах (позиція, матриця виду)
void Camera::updateCamera()
{
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	constantData->cameraPos[0] = mOwner->getTransform()->getPosition().x;
	constantData->cameraPos[1] = mOwner->getTransform()->getPosition().y;
	constantData->cameraPos[2] = mOwner->getTransform()->getPosition().z;

	Matrix view = *mOwner->getTransform()->getMatrix();

	view.inverse();

	constantData->view = view;

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}

void Camera::awake()
{
}
