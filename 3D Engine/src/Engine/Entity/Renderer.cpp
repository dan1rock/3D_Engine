#include "Renderer.h"
#include "Material.h"
#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "EntityManager.h"
#include "Entity.h"
#include "Frustum.h"

// Чи отримують рендер-компоненти власні копії матеріалів
bool Renderer::sInstancing = false;

Renderer::Renderer()
{
}

// Знімає реєстрацію рендер-компонента в EntityManager і звільняє власні копії матеріалів
Renderer::~Renderer()
{
	for (unsigned int slot = 0; slot < (unsigned int)mInstances.size(); slot++)
	{
		releaseInstance(slot);
	}

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
	if (!castShadows || mMesh == nullptr || mMesh->getIndexBuffer() == nullptr) return;

	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();
	constantData->model = *mOwner->getTransform()->getMatrix();

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();

	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	deviceContext->setVertexBuffer(mMesh->getVertexBuffer());
	deviceContext->setIndexBuffer(mMesh->getIndexBuffer());

	const std::vector<SubMesh>& subMeshes = mMesh->getSubMeshes();

	// Частини з обрізанням за альфою кидають тінь лише там, де матеріал непрозорий
	if (subMeshes.empty())
	{
		GraphicsEngine::get()->setShadowMaterial(resolveMaterial(0));
		deviceContext->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
		return;
	}

	for (const SubMesh& subMesh : subMeshes)
	{
		GraphicsEngine::get()->setShadowMaterial(resolveMaterial(subMesh.materialSlot));
		deviceContext->drawIndexedTriangleList(subMesh.indexCount, subMesh.indexStart, 0);
	}
}

// Перевіряє, чи серед матеріалів є прозорі, які малюються окремим проходом
bool Renderer::hasTransparentMaterial()
{
	unsigned int slots = getMaterialCount();

	for (unsigned int slot = 0; slot < slots; slot++)
	{
		Material* material = resolveMaterial(slot);

		if (material && material->isTransparent()) return true;
	}

	return false;
}

// Повертає найбільший порядок сортування серед прозорих матеріалів
int Renderer::getSortingPriority()
{
	int priority = -1000;
	unsigned int slots = getMaterialCount();

	for (unsigned int slot = 0; slot < slots; slot++)
	{
		Material* material = resolveMaterial(slot);

		if (material && material->isTransparent() && material->properties.sortingPriority > priority) priority = material->properties.sortingPriority;
	}

	return priority == -1000 ? 0 : priority;
}

// Повертає центр меж меша у світі, за відстанню до якого сортуються прозорі об'єкти
Vector3 Renderer::getWorldBoundsCenter()
{
	Matrix* matrix = mOwner->getTransform()->getMatrix();

	Vector3 localCenter = mMesh ? mMesh->getBoundsCenter() : Vector3();

	return Vector3(
		localCenter.x * matrix->mat[0][0] + localCenter.y * matrix->mat[1][0] + localCenter.z * matrix->mat[2][0] + matrix->mat[3][0],
		localCenter.x * matrix->mat[0][1] + localCenter.y * matrix->mat[1][1] + localCenter.z * matrix->mat[2][1] + matrix->mat[3][1],
		localCenter.x * matrix->mat[0][2] + localCenter.y * matrix->mat[1][2] + localCenter.z * matrix->mat[2][2] + matrix->mat[3][2]);
}

// Малює весь меш одним викликом тими шейдерами, що вже встановлені, без матеріалу
void Renderer::renderGeometry()
{
	// Меш без буферів не має що малювати
	if (mMesh == nullptr || mMesh->getIndexBuffer() == nullptr) return;

	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();
	constantData->model = *mOwner->getTransform()->getMatrix();

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
}

// Встановлює спільний матеріал одразу для всіх частин меша
void Renderer::setMaterial(Material* material)
{
	mSharedMaterial = material;

	// Копії старих матеріалів більше не потрібні: наступна копія зробиться вже з нового
	for (unsigned int slot = 0; slot < (unsigned int)mInstances.size(); slot++)
	{
		releaseInstance(slot);
	}
}

// Встановлює спільний матеріал для вказаного слота, тобто для частин меша з цим номером матеріалу
void Renderer::setMaterial(unsigned int slot, Material* material)
{
	if (mMaterials.size() <= slot) mMaterials.resize(slot + 1, nullptr);

	mMaterials[slot] = material;

	releaseInstance(slot);
}

// Прибирає матеріали слотів, тож усі частини меша знову малюються матеріалом за замовчуванням
void Renderer::clearMaterials()
{
	mMaterials.clear();

	setMaterial(GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial());
}

// Повертає матеріал, яким слід малювати вказаний слот: власну копію, якщо вона є, інакше спільний
Material* Renderer::resolveMaterial(unsigned int slot)
{
	if (slot < mInstances.size() && mInstances[slot]) return mInstances[slot];

	return getSharedMaterial(slot);
}

// Звільняє власну копію матеріалу слота, щоб слот знову малювався спільним
void Renderer::releaseInstance(unsigned int slot)
{
	if (slot >= mInstances.size() || mInstances[slot] == nullptr) return;

	delete mInstances[slot];
	mInstances[slot] = nullptr;
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

// Повертає матеріал слота; у режимі гри це власна копія цього рендер-компонента
Material* Renderer::getMaterial(unsigned int slot)
{
	Material* shared = getSharedMaterial(slot);

	if (!sInstancing) return shared;

	if (mInstances.size() <= slot) mInstances.resize(slot + 1, nullptr);

	if (mInstances[slot] == nullptr)
	{
		// Копія належить компоненту: звільняється разом з ним, а не зі зміною сцени
		Material* instance = new Material(*shared);
		instance->name = (shared->name.empty() ? std::string("Material") : shared->name) + " (Instance)";
		instance->dontDeleteOnLoad = true;

		mInstances[slot] = instance;
	}

	return mInstances[slot];
}

// Повертає спільний матеріал слота, яким можуть користуватися й інші об'єкти
Material* Renderer::getSharedMaterial(unsigned int slot)
{
	if (slot < mMaterials.size() && mMaterials[slot]) return mMaterials[slot];

	// Частини без власного матеріалу малюються спільним матеріалом усього меша
	if (mSharedMaterial) return mSharedMaterial;

	return GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

// Повертає кількість слотів матеріалів, які має меш рендер-компонента
unsigned int Renderer::getMaterialCount()
{
	unsigned int count = mMesh ? mMesh->getMaterialCount() : 1;

	return count > 0 ? count : 1;
}

// Вмикає власні копії матеріалів у рендер-компонентів, щоб у режимі гри правки не зачіпали файли матеріалів
void Renderer::setInstancing(bool enabled)
{
	sInstancing = enabled;
}

// Перевіряє, чи рендер-компоненти зараз отримують власні копії матеріалів
bool Renderer::isInstancing()
{
	return sInstancing;
}
