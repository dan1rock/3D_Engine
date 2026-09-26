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
	// Встановлює матеріал одразу для всіх частин меша
	void setMaterial(Material* material);
	// Встановлює матеріал для вказаного слота, тобто для частин меша з цим номером матеріалу
	void setMaterial(unsigned int slot, Material* material);
	// Встановлює меш для рендер-компонента
	void setMesh(Mesh* mesh);
	// Повертає вказівник на меш, який використовується рендер-компонентом
	Mesh* getMesh();
	// Повертає вказівник на матеріал вказаного слота
	Material* getMaterial(unsigned int slot = 0);
	// Повертає кількість слотів матеріалів, які має меш рендер-компонента
	unsigned int getMaterialCount();

	// Повертає спільний матеріал як є, без підстановки матеріалу за замовчуванням
	Material* getSharedMaterial();
	// Повертає матеріал, заданий саме для цього слота, або nullptr, якщо слот бере спільний
	Material* getSlotMaterial(unsigned int slot);
	// Повертає кількість слотів, для яких матеріал може бути заданий окремо
	unsigned int getSlotMaterialCount();

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
	// Повертає матеріал, яким слід малювати вказаний слот, з підстановкою запасних варіантів
	Material* resolveMaterial(unsigned int slot);

	// Матеріал, яким малюються всі частини меша без власного матеріалу
	Material* mSharedMaterial = nullptr;
	// Матеріали окремих слотів; порожній слот означає, що діє спільний матеріал
	std::vector<Material*> mMaterials;

	Mesh* mMesh = nullptr;

	bool isInitialized = false;
};

