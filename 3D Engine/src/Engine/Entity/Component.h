#pragma once

class Entity;
class SceneWriter;
class SceneReader;

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
	// Малює поля компонента в інспекторі редактора
	virtual void drawInspector() {}

	// Записує власні поля у файл сцени: без цього компонент відновлюється з налаштуваннями
	// за замовчуванням і втрачає як числа, так і посилання на інші об'єкти
	virtual void serialize(SceneWriter& writer) const {}
	// Відновлює власні поля з файлу сцени
	virtual void deserialize(const SceneReader& reader) {}

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
	// Викликається після зупинки гри в редакторі: компонент має забути все, що створив під час неї,
	// бо ці об'єкти вже знищено
	virtual void onEditorStop();

	friend class Entity;
	friend class EntityManager;
};

