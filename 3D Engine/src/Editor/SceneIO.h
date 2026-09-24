#pragma once
#include "Properties.h"
#include "Json.h"
#include "Vector3.h"
#include <string>
#include <vector>
#include <unordered_map>

class Entity;

// Складає з полів компонента об'єкт JSON під час збереження сцени
class SceneWriteVisitor : public PropertyVisitor
{
public:
	SceneWriteVisitor(JsonValue& out, const std::unordered_map<Entity*, int>& indices);

	void property(const char* name, float& value, float step) override;
	void property(const char* name, int& value) override;
	void property(const char* name, bool& value) override;
	void property(const char* name, Vector3& value) override;
	void property(const char* name, std::string& value) override;

	void color(const char* name, float* channels, int count) override;

	void reference(const char* name, Entity*& value, ReferenceKind kind) override;

	using PropertyVisitor::property;
	using PropertyVisitor::reference;

private:
	JsonValue& mOut;
	const std::unordered_map<Entity*, int>& mIndices;
};

// Роздає компоненту його поля з об'єкта JSON під час завантаження сцени
class SceneReadVisitor : public PropertyVisitor
{
public:
	SceneReadVisitor(const JsonValue& fields, const std::vector<Entity*>& entities);

	void property(const char* name, float& value, float step) override;
	void property(const char* name, int& value) override;
	void property(const char* name, bool& value) override;
	void property(const char* name, Vector3& value) override;
	void property(const char* name, std::string& value) override;

	void color(const char* name, float* channels, int count) override;

	void reference(const char* name, Entity*& value, ReferenceKind kind) override;

	using PropertyVisitor::property;
	using PropertyVisitor::reference;

private:
	const JsonValue& mFields;
	const std::vector<Entity*>& mEntities;
};

// Перетворює вектор на масив з трьох чисел і навпаки: так вектори лишаються в один рядок
JsonValue vectorToJson(const Vector3& value);
Vector3 jsonToVector(const JsonValue& value, const Vector3& fallback);
