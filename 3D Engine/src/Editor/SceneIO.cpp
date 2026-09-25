#include "SceneIO.h"
#include "Entity.h"
#include "PrefabLibrary.h"
#include "Prefab.h"

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

SceneWriteVisitor::SceneWriteVisitor(JsonValue& out, const std::unordered_map<Entity*, int>& indices)
	: mOut(out), mIndices(indices)
{
}

void SceneWriteVisitor::property(const char* name, float& value, float step)
{
	mOut.set(name, value);
}

void SceneWriteVisitor::property(const char* name, int& value)
{
	mOut.set(name, value);
}

void SceneWriteVisitor::property(const char* name, bool& value)
{
	mOut.set(name, value);
}

void SceneWriteVisitor::property(const char* name, Vector3& value)
{
	mOut.set(name, vectorToJson(value));
}

void SceneWriteVisitor::property(const char* name, std::string& value)
{
	// Порожній рядок не пишемо: читання все одно взяло б значення за замовчуванням
	if (value.empty()) return;

	mOut.set(name, value);
}

void SceneWriteVisitor::color(const char* name, float* channels, int count)
{
	JsonValue out = JsonValue::array();

	for (int channel = 0; channel < count; channel++)
	{
		out.push(channels[channel]);
	}

	mOut.set(name, out);
}

void SceneWriteVisitor::reference(const char* name, Entity*& value, ReferenceKind kind)
{
	if (value == nullptr) return;

	auto it = mIndices.find(value);

	if (it != mIndices.end())
	{
		mOut.set(name, it->second);
		return;
	}

	// Образ префаба не належить жодній сцені, тому на нього посилаються шляхом до файлу
	std::string assetPath = PrefabLibrary::get()->getTemplatePath(value);

	// Інший об'єкт поза записаним набором зберегти нічим, тому посилання просто не пишемо
	if (!assetPath.empty()) mOut.set(name, assetPath);
}

SceneReadVisitor::SceneReadVisitor(const JsonValue& fields, const std::vector<Entity*>& entities)
	: mFields(fields), mEntities(entities)
{
}

void SceneReadVisitor::property(const char* name, float& value, float step)
{
	value = mFields.get(name).asFloat(value);
}

void SceneReadVisitor::property(const char* name, int& value)
{
	value = mFields.get(name).asInt(value);
}

void SceneReadVisitor::property(const char* name, bool& value)
{
	value = mFields.get(name).asBool(value);
}

void SceneReadVisitor::property(const char* name, Vector3& value)
{
	value = jsonToVector(mFields.get(name), value);
}

void SceneReadVisitor::property(const char* name, std::string& value)
{
	value = mFields.get(name).asString(value);
}

void SceneReadVisitor::color(const char* name, float* channels, int count)
{
	const JsonValue& value = mFields.get(name);

	if (value.getType() != JsonValue::Type::Array) return;

	for (int channel = 0; channel < count && (size_t)channel < value.size(); channel++)
	{
		channels[channel] = value.at((size_t)channel).asFloat(channels[channel]);
	}
}

void SceneReadVisitor::reference(const char* name, Entity*& value, ReferenceKind kind)
{
	// Поля немає у файлі: лишаємо те, що компонент поставив собі сам
	if (!mFields.has(name)) return;

	const JsonValue& field = mFields.get(name);

	// Рядок — це шлях до префаба-файлу, а число — номер об'єкта в тому самому описі
	if (field.getType() == JsonValue::Type::String)
	{
		value = PrefabLibrary::get()->getTemplate(field.asString());
		return;
	}

	int index = field.asInt(-1);

	value = (index >= 0 && index < (int)mEntities.size()) ? mEntities[index] : nullptr;
}
