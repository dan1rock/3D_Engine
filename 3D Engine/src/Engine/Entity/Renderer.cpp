#include "Renderer.h"
#include "Material.h"
#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "EntityManager.h"
#include "Entity.h"
#include "Frustum.h"

Renderer::Renderer()
{
}

// Знімає реєстрацію рендер-компонента в EntityManager
Renderer::~Renderer()
{
	EntityManager::get()->unregisterRenderer(this);
}

// Реєструє компонент як звичайний компонент і як рендер-компонент в EntityManager
void Renderer::registerComponent()
{
	Component::registerComponent();
	EntityManager::get()->registerRenderer(this);
}

// Викликається при активації компонента, встановлює матеріал за замовчуванням, якщо він не заданий
void Renderer::awake()
{
	if (mMaterial == nullptr) mMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

// Викликається для рендеру об'єкта: встановлює матеріал, оновлює матриці та константний буфер
void Renderer::render()
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

// Перевіряє, чи потрапляє об'єкт у піраміду видимості
bool Renderer::isInsideFrustum(const Frustum& frustum, bool sidesOnly)
{
	// Меші без меж та позначені об'єкти рендеряться завжди
	if (alwaysVisible || mMesh == nullptr || mMesh->getBoundsRadius() <= 0.0f) return true;

	Matrix* matrix = mOwner->getTransform()->getMatrix();

	Vector3 localCenter = mMesh->getBoundsCenter();

	// Переводить центр сфери у світові координати
	Vector3 center(
		localCenter.x * matrix->mat[0][0] + localCenter.y * matrix->mat[1][0] + localCenter.z * matrix->mat[2][0] + matrix->mat[3][0],
		localCenter.x * matrix->mat[0][1] + localCenter.y * matrix->mat[1][1] + localCenter.z * matrix->mat[2][1] + matrix->mat[3][1],
		localCenter.x * matrix->mat[0][2] + localCenter.y * matrix->mat[1][2] + localCenter.z * matrix->mat[2][2] + matrix->mat[3][2]);

	// Радіус зростає разом з найбільшим з масштабів об'єкта
	Vector3 scale = mOwner->getTransform()->getScale();

	float maxScale = fabsf(scale.x);
	if (fabsf(scale.y) > maxScale) maxScale = fabsf(scale.y);
	if (fabsf(scale.z) > maxScale) maxScale = fabsf(scale.z);

	return frustum.intersects(center, mMesh->getBoundsRadius() * maxScale, sidesOnly);
}

// Викликається під час проходу карти тіней: рендерить лише глибину меша
void Renderer::renderDepth()
{
	if (!castShadows || mMesh == nullptr) return;

	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();
	constantData->model = *mOwner->getTransform()->getMatrix();

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
}

// Встановлює матеріал для рендер-компонента
void Renderer::setMaterial(Material* material)
{
	this->mMaterial = material;
}

// Встановлює меш для рендер-компонента
void Renderer::setMesh(Mesh* mesh)
{
	this->mMesh = mesh;
}

// Повертає вказівник на меш, який використовується рендер-компонентом
Mesh* Renderer::getMesh()
{
	return mMesh;
}

Material* Renderer::getMaterial()
{
	return mMaterial;
}
