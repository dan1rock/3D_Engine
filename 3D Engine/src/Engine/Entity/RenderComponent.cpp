#include "RenderComponent.h"
#include "Material.h"
#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "EntityManager.h"
#include "Entity.h"

RenderComponent::RenderComponent()
{
}

// Знімає реєстрацію рендер-компонента в EntityManager
RenderComponent::~RenderComponent()
{
	EntityManager::get()->unregisterRenderer(this);
}

// Реєструє компонент як звичайний компонент і як рендер-компонент в EntityManager
void RenderComponent::registerComponent()
{
	Component::registerComponent();
	EntityManager::get()->registerRenderer(this);
}

// Викликається при активації компонента, встановлює матеріал за замовчуванням, якщо він не заданий
void RenderComponent::awake()
{
	if (mMaterial == nullptr) mMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

// Викликається для рендеру об'єкта: встановлює матеріал, оновлює матриці та константний буфер
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

// Встановлює матеріал для рендер-компонента
void RenderComponent::setMaterial(Material* material)
{
	this->mMaterial = material;
}

// Встановлює меш для рендер-компонента
void RenderComponent::setMesh(Mesh* mesh)
{
	this->mMesh = mesh;
}

// Повертає вказівник на меш, який використовується рендер-компонентом
Mesh* RenderComponent::getMesh()
{
	return mMesh;
}
