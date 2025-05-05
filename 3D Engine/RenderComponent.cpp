#include "RenderComponent.h"
#include "Material.h"
#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "ComponentManager.h"
#include "GameObject.h"

RenderComponent::RenderComponent()
{
	ComponentManager::get()->registerRenderer(this);
}

RenderComponent::~RenderComponent()
{
	ComponentManager::get()->unregisterRenderer(this);
}

void RenderComponent::awake()
{
	if (mMaterial == nullptr) mMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

void RenderComponent::render()
{
	GraphicsEngine::get()->setMaterial(mMaterial);
	
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();
	constantData->model = *mOwner->getTransform()->getMatrix();

	Matrix invTransModel = *mOwner->getTransform()->getMatrix();
	invTransModel.inverse();
	invTransModel.transpose();
	constantData->invTransModel = invTransModel;

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}

void RenderComponent::setMaterial(Material* material)
{
	this->mMaterial = material;
}

void RenderComponent::setMesh(Mesh* mesh)
{
	this->mMesh = mesh;
}

Mesh* RenderComponent::getMesh()
{
	return mMesh;
}
