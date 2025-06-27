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
	// Повертає вказівник на локальну матрицю трансформації
	Matrix* getLocalMatrix();
	// Повертає позицію об'єкта
	Vector3 getPosition();
	// Повертає локальну позицію об'єкта
	Vector3 getLocalPosition();
	// Повертає масштаб об'єкта
	Vector3 getScale();
	// Повертає локальний масштаб об'єкта
	Vector3 getLocalScale();
	// Повертає обертання об'єкта
	Vector3 getRotation();
	// Повертає локальне обертання об'єкта
	Vector3 getLocalRotation();
	// Повертає напрямок "вперед" (Z) у світових координатах
	Vector3 getForward();
	// Повертає напрямок "вправо" (X) у світових координатах
	Vector3 getRight();
	// Повертає напрямок "вгору" (Y) у світових координатах
	Vector3 getUp();
	
	// Встановлює матрицю трансформації, за потреби оновлює фізичне тіло
	void setMatrix(const Matrix& matrix, bool updateRb = true);
	// Встановлює позицію об'єкта, за потреби оновлює фізичне тіло
	void setPosition(Vector3 position, bool updateRb = true);
	// Встановлює локальну позицію об'єкта, за потреби оновлює фізичне тіло
	void setLocalPosition(Vector3 position, bool updateRb = true);
	// Встановлює масштаб об'єкта
	void setScale(Vector3 scale);
	// Встановлює локальний масштаб об'єкта
	void setLocalScale(Vector3 scale);
	// Встановлює обертання об'єкта, за потреби оновлює фізичне тіло
	void setRotation(Vector3 rotation, bool updateRb = true);
	// Встановлює локальне обертання об'єкта, за потреби оновлює фізичне тіло
	void setLocalRotation(Vector3 rotation, bool updateRb = true);
	// Встановлює напрямок "вперед" (Z) для об'єкта, за потреби оновлює фізичне тіло
	void setForward(Vector3 forward, bool updateRb = true);

protected:
	// Конструктор копіювання
	Transform* instantiate() const override
	{
		return new Transform(*this);
	}

private:
	void updateGlobalMatrix(bool updateRb = true);
	void updateChildren(bool updateRb = true);

	Matrix mMatrix = {};
	Matrix mLocalMatrix = {};

	Vector3 mPosition = {};
	Vector3 mScale = { 1.0f, 1.0f, 1.0f };
	Vector3 mRotation = { 0.0f, 0.0f, 0.0f };

	Vector3 mLocalPosition = {};
	Vector3 mLocalScale = { 1.0f, 1.0f, 1.0f };
	Vector3 mLocalRotation = { 0.0f, 0.0f, 0.0f };

	friend class Entity;
};

