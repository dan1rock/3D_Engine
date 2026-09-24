#include "SceneIO.h"
#include "Entity.h"
#include "Prefab.h"
#include "Transform.h"

// Перетворює вектор на масив з трьох чисел і навпаки: так вектори лишаються в один рядок
JsonValue vectorToJson(const Vector3& value)
{
	JsonValue out = JsonValue::array();

	out.push(value.x);
	out.push(value.y);
	out.push(value.z);

	return out;
}

// Перетворює вектор на масив з трьох чисел і навпаки: так вектори лишаються в один рядок
Vector3 jsonToVector(const JsonValue& value, const Vector3& fallback)
{
	if (value.getType() != JsonValue::Type::Array || value.size() < 3) return fallback;

	return Vector3(
		value.at(0).asFloat(fallback.x),
		value.at(1).asFloat(fallback.y),
		value.at(2).asFloat(fallback.z));
}

SceneWriter::SceneWriter(JsonValue& out, const std::unordered_map<Entity*, int>& indices)
	: mOut(out), mIndices(indices)
{
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, float value)
{
	mOut.set(key, value);
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, int value)
{
	mOut.set(key, value);
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, bool value)
{
	mOut.set(key, value);
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, const Vector3& value)
{
	mOut.set(key, vectorToJson(value));
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, const std::string& value)
{
	// Порожній рядок не пишемо: читання все одно взяло б значення за замовчуванням
	if (value.empty()) return;

	mOut.set(key, value);
}

// Посилання на інший об'єкт сцени зберігається його номером: вказівник після перезапуску не діє
void SceneWriter::writeRef(const char* key, Entity* entity)
{
	if (entity == nullptr) return;

	auto it = mIndices.find(entity);

	// Об'єкт поза сценою зберегти нічим, тому посилання просто не записуємо
	if (it == mIndices.end()) return;

	mOut.set(key, it->second);
}

// Посилання на інший об'єкт сцени зберігається його номером: вказівник після перезапуску не діє
void SceneWriter::writeRef(const char* key, Transform* transform)
{
	if (transform == nullptr) return;

	writeRef(key, transform->getOwner());
}

SceneReader::SceneReader(const JsonValue& fields, const std::vector<Entity*>& entities)
	: mFields(fields), mEntities(entities)
{
}

// Перевіряє, чи є таке поле у файлі: інакше компонент лишає своє значення за замовчуванням
bool SceneReader::has(const char* key) const
{
	return mFields.has(key);
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
float SceneReader::read(const char* key, float fallback) const
{
	return mFields.get(key).asFloat(fallback);
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
int SceneReader::read(const char* key, int fallback) const
{
	return mFields.get(key).asInt(fallback);
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
bool SceneReader::read(const char* key, bool fallback) const
{
	return mFields.get(key).asBool(fallback);
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
Vector3 SceneReader::read(const char* key, const Vector3& fallback) const
{
	return jsonToVector(mFields.get(key), fallback);
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
std::string SceneReader::read(const char* key, const std::string& fallback) const
{
	return mFields.get(key).asString(fallback);
}

// Відновлює посилання на об'єкт сцени за збереженим номером
Entity* SceneReader::readEntity(const char* key) const
{
	int index = read(key, -1);

	if (index < 0 || index >= (int)mEntities.size()) return nullptr;

	return mEntities[index];
}

// Відновлює посилання на об'єкт сцени за збереженим номером
Prefab* SceneReader::readPrefab(const char* key) const
{
	// Образ відрізняється від звичайного об'єкта типом, тому перевіряємо його явно
	return dynamic_cast<Prefab*>(readEntity(key));
}

// Відновлює посилання на об'єкт сцени за збереженим номером
Transform* SceneReader::readTransform(const char* key) const
{
	Entity* entity = readEntity(key);

	if (entity == nullptr) return nullptr;

	return entity->getTransform();
}
