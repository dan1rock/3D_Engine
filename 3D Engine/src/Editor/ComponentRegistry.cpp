#include "ComponentRegistry.h"
#include "Entity.h"
#include "MeshRenderer.h"
#include "SkySphere.h"
#include "Camera.h"
#include "DirectionalLight.h"
#include "RigidBody.h"
#include "MeshCollider.h"
#include "SphereCollider.h"
#include "PlaneCollider.h"

#include <unordered_map>

// Таблиця створення компонентів за іменем типу
static std::unordered_map<std::string, ComponentRegistry::Creator>& creators()
{
	static std::unordered_map<std::string, ComponentRegistry::Creator> map;
	return map;
}

// Порядок реєстрації зберігається окремо, щоб меню редактора було передбачуваним
static std::vector<std::string>& names()
{
	static std::vector<std::string> list;
	return list;
}

// Реєструє тип компонента під вказаним іменем
void ComponentRegistry::registerType(const std::string& name, Creator creator)
{
	if (creators().find(name) != creators().end()) return;

	creators()[name] = creator;
	names().push_back(name);
}

// Створює компонент зареєстрованого типу, або повертає nullptr для невідомого імені
Component* ComponentRegistry::create(const std::string& name, Entity* entity)
{
	auto it = creators().find(name);

	if (it == creators().end()) return nullptr;

	return it->second(entity);
}

// Повертає імена всіх зареєстрованих типів
const std::vector<std::string>& ComponentRegistry::getTypeNames()
{
	return names();
}

// Реєструє типи компонентів, які входять до складу рушія
void ComponentRegistry::registerEngineTypes()
{
	registerType("MeshRenderer", [](Entity* e) -> Component* { return e->addComponent<MeshRenderer>(); });
	registerType("SkySphere", [](Entity* e) -> Component* { return e->addComponent<SkySphere>(); });
	registerType("Camera", [](Entity* e) -> Component* { return e->addComponent<Camera>(); });
	registerType("DirectionalLight", [](Entity* e) -> Component* { return e->addComponent<DirectionalLight>(); });
	registerType("MeshCollider", [](Entity* e) -> Component* { return e->addComponent<MeshCollider>(); });
	registerType("SphereCollider", [](Entity* e) -> Component* { return e->addComponent<SphereCollider>(); });
	registerType("PlaneCollider", [](Entity* e) -> Component* { return e->addComponent<PlaneCollider>(); });

	// Фізичне тіло за замовчуванням нерухоме: рухоме потребує маси, яку задають в інспекторі
	registerType("RigidBody", [](Entity* e) -> Component* { return e->addComponent<RigidBody>(true); });
}
