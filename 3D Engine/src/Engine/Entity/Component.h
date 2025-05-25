#pragma once

class Entity;

// Клас, що представляє компонент в ігровому об'єкті, базовий клас для всіх компонентів
class Component
{
public:
	Component();
	// Автоматично знімає реєстрацію компонента в EntityManager
	virtual ~Component();

	// Повертає вказівник на об'єкт-власник (GameObject) цього компонента
	Entity* getOwner();

protected:
	Entity* mOwner = nullptr;

	// Метод для створення копії компонента, реалізується в похідних класах
	virtual Component* instantiate() const = 0;
	// Реєструє компонент в EntityManager
	virtual void registerComponent();

private:
	// Встановлює власника (GameObject) для цього компонента
	void setOwner(Entity* gameObject);

	// Викликається при активації компонента
	virtual void awake();
	// Оновлення компонента (викликається кожен кадр)
	virtual void update();
	// Фіксоване оновлення компонента (викликається з фіксованим кроком часу)
	virtual void fixedUpdate();

	friend class Entity;
	friend class EntityManager;
};

