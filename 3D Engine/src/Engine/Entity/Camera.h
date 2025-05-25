#pragma once
#include "Component.h"

// Копонент камери, що відповідає за позицію точки зору в сцені
class Camera : public Component
{
public:
	Camera();
	// Деструктор класу Camera, знімає реєстрацію камери в EntityManager
	~Camera() override;

protected:
	// Конструктор копіювання
	Camera* instantiate() const override {
		return new Camera(*this);
	};

	// Реєструє компонент як звичайний компонент і як камеру в EntityManager
	void registerComponent() override;

private:
	// Оновлює дані камери у глобальних константах (позиція, матриця виду)
	void updateCamera();
	void awake() override;

	friend class EntityManager;
};

