#pragma once
#include "Component.h"
#include "Matrix.h"

// Компонент, відповідальний за трансформацію об'єкта в просторі
class Transform : public Component
{
public:
	// Ініціалізує модельну матрицю як одиничну
	Transform();
	~Transform() override;

	// Повертає вказівник на матрицю трансформації
	Matrix* getMatrix();
	// Повертає позицію об'єкта
	Vector3 getPosition();
	// Повертає масштаб об'єкта
	Vector3 getScale();
	// Повертає обертання об'єкта
	Vector3 getRotation();
	// Повертає напрямок "вперед" (Z) у світових координатах
	Vector3 getForward();
	// Повертає напрямок "вправо" (X) у світових координатах
	Vector3 getRight();
	// Повертає напрямок "вгору" (Y) у світових координатах
	Vector3 getUp();

	// Встановлює позицію об'єкта, за потреби оновлює фізичне тіло
	void setPosition(Vector3 position, bool updateRb = true);
	// Встановлює масштаб об'єкта
	void setScale(Vector3 scale);
	// Встановлює обертання об'єкта, за потреби оновлює фізичне тіло
	void setRotation(Vector3 rotation, bool updateRb = true);
	// Встановлює напрямок "вперед" (Z) для об'єкта, за потреби оновлює фізичне тіло
	void setForward(Vector3 forward, bool updateRb = true);

protected:
	// Конструктор копіювання
	Transform* instantiate() const override
	{
		return new Transform(*this);
	}

private:
	Matrix mMatrix = {};

	Vector3 mPosition = {};
	Vector3 mScale = { 1.0f, 1.0f, 1.0f };
	Vector3 mRotation = { 0.0f, 0.0f, 0.0f };
};

