#pragma once
#include "Vector3.h"
#include <string>

class Entity;
class Prefab;
class Transform;

// Який об'єкт можна покласти у поле-посилання: інспектор за цим відбирає список вибору
enum class ReferenceKind
{
	Any,
	PrefabOnly
};

// Обхідник полів компонента. Компонент перелічує свої поля один раз, а вже обхідник вирішує,
// що з ними робити: записати у файл сцени, прочитати з нього або намалювати в інспекторі.
// Завдяки цьому поле не може зберігатися, але не показуватись у редакторі, і навпаки
class PropertyVisitor
{
public:
	virtual ~PropertyVisitor() {}

	// Число з плаваючою комою; step задає, наскільки швидко значення тягнеться мишею
	virtual void property(const char* name, float& value, float step) = 0;
	virtual void property(const char* name, int& value) = 0;
	virtual void property(const char* name, bool& value) = 0;
	virtual void property(const char* name, Vector3& value) = 0;
	virtual void property(const char* name, std::string& value) = 0;

	// Колір зручніше правити палітрою, ніж трьома окремими числами
	virtual void color(const char* name, float* channels, int count) = 0;

	// Посилання на інший об'єкт сцени
	virtual void reference(const char* name, Entity*& value, ReferenceKind kind) = 0;

	// Поля між цими викликами показуються, але не редагуються: так видно значення,
	// які задаються лише під час створення компонента
	virtual void beginReadOnly() {}
	virtual void endReadOnly() {}

	// Скорочення, щоб компонент не писав крок і вид посилання там, де вони й так очевидні
	void property(const char* name, float& value) { property(name, value, 0.05f); }

	void reference(const char* name, Entity*& value) { reference(name, value, ReferenceKind::Any); }
	void reference(const char* name, Prefab*& value);
	void reference(const char* name, Transform*& value);
};
