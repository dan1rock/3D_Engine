#pragma once
#include "Json.h"
#include "Vector3.h"
#include <string>
#include <vector>
#include <unordered_map>

class Entity;
class Prefab;
class Transform;

// Приймає поля компонента під час збереження сцени та складає з них об'єкт JSON
class SceneWriter
{
public:
	SceneWriter(JsonValue& out, const std::unordered_map<Entity*, int>& indices);

	// Записує одне поле компонента під вказаним ключем
	void write(const char* key, float value);
	void write(const char* key, int value);
	void write(const char* key, bool value);
	void write(const char* key, const Vector3& value);
	void write(const char* key, const std::string& value);

	// Посилання на інший об'єкт сцени зберігається його номером: вказівник після перезапуску не діє
	void writeRef(const char* key, Entity* entity);
	void writeRef(const char* key, Transform* transform);

private:
	JsonValue& mOut;
	const std::unordered_map<Entity*, int>& mIndices;
};

// Віддає компоненту його поля під час завантаження сцени
class SceneReader
{
public:
	SceneReader(const JsonValue& fields, const std::vector<Entity*>& entities);

	// Перевіряє, чи є таке поле у файлі: інакше компонент лишає своє значення за замовчуванням
	bool has(const char* key) const;

	// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
	float read(const char* key, float fallback) const;
	int read(const char* key, int fallback) const;
	bool read(const char* key, bool fallback) const;
	Vector3 read(const char* key, const Vector3& fallback) const;
	std::string read(const char* key, const std::string& fallback) const;

	// Відновлює посилання на об'єкт сцени за збереженим номером
	Entity* readEntity(const char* key) const;
	Prefab* readPrefab(const char* key) const;
	Transform* readTransform(const char* key) const;

private:
	const JsonValue& mFields;
	const std::vector<Entity*>& mEntities;
};

// Перетворює вектор на масив з трьох чисел і навпаки: так вектори лишаються в один рядок
JsonValue vectorToJson(const Vector3& value);
Vector3 jsonToVector(const JsonValue& value, const Vector3& fallback);
