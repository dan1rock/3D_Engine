#include "SceneIO.h"
#include "Entity.h"
#include "Prefab.h"
#include "Transform.h"

#include <sstream>
#include <iomanip>
#include <limits>

SceneWriter::SceneWriter(std::string& out, const std::unordered_map<Entity*, int>& indices)
	: mOut(out), mIndices(indices)
{
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, float value)
{
	std::ostringstream line;
	line << std::setprecision(std::numeric_limits<float>::max_digits10);
	line << key << " " << value << "\n";
	mOut += line.str();
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, int value)
{
	std::ostringstream line;
	line << std::setprecision(std::numeric_limits<float>::max_digits10);
	line << key << " " << value << "\n";
	mOut += line.str();
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, bool value)
{
	write(key, value ? 1 : 0);
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, const Vector3& value)
{
	std::ostringstream line;
	line << std::setprecision(std::numeric_limits<float>::max_digits10);
	line << key << " " << value.x << " " << value.y << " " << value.z << "\n";
	mOut += line.str();
}

// Записує одне поле компонента під вказаним ключем
void SceneWriter::write(const char* key, const std::string& value)
{
	// Порожній рядок не пишемо: читання все одно взяло б значення за замовчуванням
	if (value.empty()) return;

	mOut += std::string(key) + " " + value + "\n";
}

// Посилання на інший об'єкт сцени зберігається його номером: вказівник після перезапуску не діє
void SceneWriter::writeRef(const char* key, Entity* entity)
{
	if (entity == nullptr) return;

	auto it = mIndices.find(entity);

	// Об'єкт поза сценою зберегти нічим, тому посилання просто не записуємо
	if (it == mIndices.end()) return;

	write(key, it->second);
}

// Посилання на інший об'єкт сцени зберігається його номером: вказівник після перезапуску не діє
void SceneWriter::writeRef(const char* key, Transform* transform)
{
	if (transform == nullptr) return;

	writeRef(key, transform->getOwner());
}

SceneReader::SceneReader(const std::unordered_map<std::string, std::string>& fields, const std::vector<Entity*>& entities)
	: mFields(fields), mEntities(entities)
{
}

// Повертає рядок значення або nullptr, якщо поля немає
const std::string* SceneReader::find(const char* key) const
{
	auto it = mFields.find(key);

	if (it == mFields.end()) return nullptr;

	return &it->second;
}

// Перевіряє, чи є таке поле у файлі: інакше компонент лишає своє значення за замовчуванням
bool SceneReader::has(const char* key) const
{
	return find(key) != nullptr;
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
float SceneReader::read(const char* key, float fallback) const
{
	const std::string* value = find(key);

	if (value == nullptr) return fallback;

	std::istringstream in(*value);
	float result = fallback;
	in >> result;

	return result;
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
int SceneReader::read(const char* key, int fallback) const
{
	const std::string* value = find(key);

	if (value == nullptr) return fallback;

	std::istringstream in(*value);
	int result = fallback;
	in >> result;

	return result;
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
bool SceneReader::read(const char* key, bool fallback) const
{
	return read(key, fallback ? 1 : 0) != 0;
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
Vector3 SceneReader::read(const char* key, const Vector3& fallback) const
{
	const std::string* value = find(key);

	if (value == nullptr) return fallback;

	std::istringstream in(*value);
	Vector3 result = fallback;
	in >> result.x >> result.y >> result.z;

	return result;
}

// Читає одне поле компонента, повертаючи запасне значення, якщо його немає у файлі
std::string SceneReader::read(const char* key, const std::string& fallback) const
{
	const std::string* value = find(key);

	if (value == nullptr) return fallback;

	return *value;
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
