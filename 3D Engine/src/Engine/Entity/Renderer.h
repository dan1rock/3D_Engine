#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include "Component.h"

class ConstantBuffer;
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
	// Встановлює матеріал для рендер-компонента
	void setMaterial(Material* material);
	// Встановлює меш для рендер-компонента
	void setMesh(Mesh* mesh);
	// Повертає вказівник на меш, який використовується рендер-компонентом
	Mesh* getMesh();
	// Повертає вказівник на матеріал, який використовується рендер-компонентом
	Material* getMaterial();

protected:
	// Реєструє компонент як звичайний компонент і як рендер-компонент в EntityManager
	void registerComponent() override;

	// Викликається при активації компонента, встановлює матеріал за замовчуванням, якщо він не заданий
	virtual void awake() override;

	Material* mMaterial = nullptr;

	Mesh* mMesh = nullptr;

	bool isInitialized = false;
};

