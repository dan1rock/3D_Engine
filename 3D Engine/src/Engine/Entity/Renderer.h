#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include "Component.h"
#include <vector>

class ConstantBuffer;
class Frustum;
class Material;
class Matrix;

// Базовий клас для всіх компонентів, які рендерять об'єкти
class Renderer : public Component
{
public:
	Renderer();
	// Знімає реєстрацію рендер-компонента в EntityManager
	~Renderer() override;

	// Викликається для рендеру об'єкта: встановлює матеріал, оновлює матриці та константний буфер
	virtual void render() = 0;
	// Викликається під час проходу карти тіней: рендерить лише глибину меша
	virtual void renderDepth();
	// Малює весь меш одним викликом тими шейдерами, що вже встановлені, без матеріалу
	void renderGeometry();

	// Перевіряє, чи серед матеріалів є прозорі, які малюються окремим проходом
	bool hasTransparentMaterial();
	// Повертає найбільший порядок сортування серед прозорих матеріалів
	int getSortingPriority();
	// Повертає центр меж меша у світі, за відстанню до якого сортуються прозорі об'єкти
	Vector3 getWorldBoundsCenter();
	// Встановлює спільний матеріал одразу для всіх частин меша
	void setMaterial(Material* material);
	// Встановлює спільний матеріал для вказаного слота, тобто для частин меша з цим номером матеріалу
	void setMaterial(unsigned int slot, Material* material);
	// Прибирає матеріали слотів, тож усі частини меша знову малюються матеріалом за замовчуванням
	void clearMaterials();
	// Встановлює меш для рендер-компонента
	void setMesh(Mesh* mesh);
	// Повертає вказівник на меш, який використовується рендер-компонентом
	Mesh* getMesh();
	// Повертає матеріал слота; у режимі гри це власна копія цього рендер-компонента
	Material* getMaterial(unsigned int slot = 0);
	// Повертає спільний матеріал слота, яким можуть користуватися й інші об'єкти
	Material* getSharedMaterial(unsigned int slot = 0);
	// Повертає кількість слотів матеріалів, які має меш рендер-компонента
	unsigned int getMaterialCount();

	// Вмикає власні копії матеріалів у рендер-компонентів, щоб у режимі гри правки не зачіпали файли матеріалів
	static void setInstancing(bool enabled);
	// Перевіряє, чи рендер-компоненти зараз отримують власні копії матеріалів
	static bool isInstancing();

	// Перевіряє, чи потрапляє об'єкт у піраміду видимості
	bool isInsideFrustum(const Frustum& frustum, bool sidesOnly = false);

	// Чи повинен об'єкт кидати тінь
	bool castShadows = true;
	// Об'єкт, який ніколи не відсікається за пірамідою видимості
	bool alwaysVisible = false;

protected:
	// Реєструє компонент як звичайний компонент і як рендер-компонент в EntityManager
	void registerComponent() override;

	// Викликається при активації компонента, встановлює матеріал за замовчуванням, якщо він не заданий
	virtual void awake() override;

	// Встановлює у конвеєр матеріал вказаного слота
	void applyMaterial(unsigned int slot);
	// Повертає матеріал, яким слід малювати вказаний слот: власну копію, якщо вона є, інакше спільний
	Material* resolveMaterial(unsigned int slot);
	// Звільняє власну копію матеріалу слота, щоб слот знову малювався спільним
	void releaseInstance(unsigned int slot);

	// Матеріал, яким малюються всі частини меша без власного матеріалу
	Material* mSharedMaterial = nullptr;
	// Матеріали окремих слотів; порожній слот означає, що діє спільний матеріал
	std::vector<Material*> mMaterials;
	// Власні копії матеріалів слотів у режимі гри; належать компоненту і звільняються разом з ним
	std::vector<Material*> mInstances;

	// Чи отримують рендер-компоненти власні копії матеріалів
	static bool sInstancing;

	Mesh* mMesh = nullptr;

	bool isInitialized = false;
};

