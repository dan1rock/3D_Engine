#pragma once
#include "GameObject.h"

// Образ об'єкта сцени, який можна використовувати для створення нових об'єктів
class Prefab : public GameObject
{
public:
	// Задання статусу активності об'єкта як неактивного
	Prefab();
	// Задання статусу активності об'єкта як неактивного з позицією
	Prefab(Vector3 position);
	~Prefab() override;

protected:
	// Образ не повинен прокидати компоненти при створенні
	bool shouldAwakeComponents() const override {
		return false;
	}
};

