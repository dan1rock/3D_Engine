#include "Camera.h"
#include "GameObject.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "ComponentManager.h"

Camera::Camera()
{
	GraphicsEngine::get()->getComponentManager()->registerCamera(this);
}

Camera::~Camera()
{
	GraphicsEngine::get()->getComponentManager()->unregisterCamera(this);
}

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
