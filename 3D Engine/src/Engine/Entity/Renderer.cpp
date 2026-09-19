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
	if (mSharedMaterial == nullptr) mSharedMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

// Викликається для рендеру об'єкта: оновлює матриці та константний буфер
void Renderer::render()
{
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

// Встановлює матеріал одразу для всіх частин меша
void Renderer::setMaterial(Material* material)
{
	mSharedMaterial = material;
}

// Встановлює матеріал для вказаного слота, тобто для частин меша з цим номером матеріалу
void Renderer::setMaterial(unsigned int slot, Material* material)
{
	if (mMaterials.size() <= slot) mMaterials.resize(slot + 1, nullptr);

	mMaterials[slot] = material;
}

// Повертає матеріал, яким слід малювати вказаний слот, з підстановкою запасних варіантів
Material* Renderer::resolveMaterial(unsigned int slot)
{
	if (slot < mMaterials.size() && mMaterials[slot]) return mMaterials[slot];

	// Частини без власного матеріалу малюються спільним матеріалом
	if (mSharedMaterial) return mSharedMaterial;

	return GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

// Встановлює у конвеєр матеріал вказаного слота
void Renderer::applyMaterial(unsigned int slot)
{
	GraphicsEngine::get()->setMaterial(resolveMaterial(slot));
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

// Повертає вказівник на матеріал вказаного слота
Material* Renderer::getMaterial(unsigned int slot)
{
	return resolveMaterial(slot);
}

// Повертає кількість слотів матеріалів, які має меш рендер-компонента
unsigned int Renderer::getMaterialCount()
{
	return mMesh ? mMesh->getMaterialCount() : 1;
}
