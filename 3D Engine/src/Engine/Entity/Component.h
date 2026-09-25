#pragma once

class Entity;
class PropertyVisitor;

// Оголошує ім'я типу компонента, за яким редактор і файл сцени його впізнають
#define COMPONENT_TYPE(typeName) const char* getTypeName() const override { return #typeName; }

// Клас, що представляє компонент в ігровому об'єкті, базовий клас для всіх компонентів
class Component
{
public:
	Component();
	// Автоматично знімає реєстрацію компонента в EntityManager
	virtual ~Component();

	// Повертає вказівник на об'єкт-власник (GameObject) цього компонента
	Entity* getOwner();

	// Повертає ім'я типу компонента для редактора та збереження сцени
	virtual const char* getTypeName() const { return "Component"; }
	// Перелічує власні поля компонента. Один список обслуговує і збереження сцени, і інспектор:
	// без нього компонент відновлюється з налаштуваннями за замовчуванням, втрачає посилання
	// на інші об'єкти і не має що показати в редакторі
	virtual void visitProperties(PropertyVisitor& visitor) {}

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
	// Викликається, коли з того самого об'єкта прибирають інший компонент, ще до його знищення:
	// хто тримав на нього вказівник, має його забути
	virtual void onComponentRemoved(Component* removed);

	friend class Entity;
	friend class EntityManager;
};

