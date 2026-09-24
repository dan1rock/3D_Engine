#include "SceneSerializer.h"
#include "SceneIO.h"
#include "Json.h"
#include "ComponentRegistry.h"
#include "EntityManager.h"
#include "Entity.h"
#include "Prefab.h"
#include "Renderer.h"
#include "MeshRenderer.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "DirectionalLight.h"
#include "RigidBody.h"
#include "GraphicsEngine.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <iostream>

// Номер формату: змінюється, коли файли старих версій більше не читаються так само
static const int SCENE_VERSION = 3;

// Переводить вузький рядок у широкий, бо шляхи ресурсів рушій приймає як wchar_t
static std::wstring toWide(const std::string& text)
{
	return std::wstring(text.begin(), text.end());
}

// Переводить широкий рядок у вузький для запису у файл сцени
static std::string toNarrow(const std::wstring& text)
{
	return std::string(text.begin(), text.end());
}

// Повертає шлях ресурсу відносно теки проєкту, щоб сцена не залежала від розташування на диску
static std::string toRelativePath(const std::wstring& fullPath)
{
	std::string path = toNarrow(fullPath);

	size_t assets = path.rfind("Assets");

	if (assets != std::string::npos) return path.substr(assets);

	// Шейдери лежать поруч з кодом, тому для них орієнтиром є тека src
	size_t source = path.rfind("src");

	if (source != std::string::npos) return path.substr(source);

	return path;
}

// Складає опис одного матеріалу рендер-компонента
static JsonValue writeMaterial(Material* material, int slot)
{
	JsonValue out = JsonValue::object();

	out.set("slot", slot);

	JsonValue color = JsonValue::array();
	color.push(material->color[0]);
	color.push(material->color[1]);
	color.push(material->color[2]);
	color.push(material->color[3]);

	out.set("color", color);
	out.set("ambient", material->ambient);
	out.set("smoothness", material->smoothness);
	out.set("shininess", material->shininess);
	out.set("textureScale", material->textureScale);
	out.set("cullBack", material->cullBack);
	out.set("clampTexture", material->clampTexture);

	std::wstring texturePath = material->getTexturePath();

	if (!texturePath.empty()) out.set("texture", toRelativePath(texturePath));

	// Нестандартний піксельний шейдер треба зберегти: інакше матеріал відновиться зі звичайним,
	// а той по-іншому змішує текстуру з кольором і малює, наприклад, прототипну сітку чорною
	std::wstring shaderPath = material->getPixelShaderPath();

	if (!shaderPath.empty()) out.set("shader", toRelativePath(shaderPath));

	return out;
}

// Записує поточну сцену у текст
std::string SceneSerializer::serialize()
{
	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	// Індекс кожного об'єкта потрібен, щоб зберегти зв'язки батько-дитина та посилання компонентів
	std::unordered_map<Entity*, int> indices;

	int index = 0;

	for (Entity* entity : entities)
	{
		// Об'єкти, що переживають зміну сцени, не зберігаються: інакше завантаження створить їх копію
		if (entity->dontDestroyOnLoad) continue;

		indices[entity] = index++;
	}

	JsonValue entityList = JsonValue::array();

	for (Entity* entity : entities)
	{
		if (entity->dontDestroyOnLoad) continue;

		JsonValue entityValue = JsonValue::object();

		entityValue.set("index", indices[entity]);
		entityValue.set("name", entity->getName());
		entityValue.set("active", entity->isActiveSelf);

		// Образ треба відрізнити від звичайного об'єкта, бо інакше він оживе як окремий об'єкт сцени
		// і водночас компоненти, що на нього посилаються, лишаться без образу для створення копій
		if (dynamic_cast<Prefab*>(entity) != nullptr) entityValue.set("prefab", true);

		Entity* parent = entity->getParent();

		// Кореневий об'єкт не має відносно чого зберігати локальні координати, тому пишемо світові
		bool hasParent = parent != nullptr && indices.count(parent) > 0;

		entityValue.set("parent", hasParent ? indices[parent] : -1);

		Transform* transform = entity->getTransform();

		JsonValue transformValue = JsonValue::object();

		transformValue.set("position", vectorToJson(hasParent ? transform->getLocalPosition() : transform->getPosition()));
		transformValue.set("rotation", vectorToJson(hasParent ? transform->getLocalRotation() : transform->getRotation()));
		transformValue.set("scale", vectorToJson(hasParent ? transform->getLocalScale() : transform->getScale()));

		entityValue.set("transform", transformValue);

		JsonValue componentList = JsonValue::array();

		for (Component* component : entity->getComponentList())
		{
			JsonValue componentValue = JsonValue::object();

			componentValue.set("type", std::string(component->getTypeName()));

			// Рендер-компонент зберігає свій меш та матеріали кожного слота
			if (Renderer* renderer = dynamic_cast<Renderer*>(component))
			{
				if (renderer->getMesh())
				{
					componentValue.set("mesh", toRelativePath(renderer->getMesh()->getFullPath()));
				}

				componentValue.set("castShadows", renderer->castShadows);

				JsonValue materialList = JsonValue::array();

				unsigned int slots = renderer->getMaterialCount();

				for (unsigned int slot = 0; slot < slots; slot++)
				{
					if (Material* material = renderer->getMaterial(slot))
					{
						materialList.push(writeMaterial(material, (int)slot));
					}
				}

				if (materialList.size() > 0) componentValue.set("materials", materialList);
			}

			// Решту полів компонент записує сам, бо лише він знає, що саме варто зберігати
			SceneWriter writer(componentValue, indices);
			component->serialize(writer);

			componentList.push(componentValue);
		}

		if (componentList.size() > 0) entityValue.set("components", componentList);

		entityList.push(entityValue);
	}

	JsonValue scene = JsonValue::object();

	scene.set("version", SCENE_VERSION);
	scene.set("entities", entityList);

	return scene.toString();
}

// Створює матеріал одного слота рендер-компонента
static void buildMaterial(Renderer* renderer, const JsonValue& data)
{
	// Кожен слот отримує власний матеріал, щоб правки не розповзалися між об'єктами
	Material* material = new Material();

	const JsonValue& color = data.get("color");

	if (color.getType() == JsonValue::Type::Array && color.size() >= 4)
	{
		for (int channel = 0; channel < 4; channel++)
		{
			material->color[channel] = color.at((size_t)channel).asFloat(material->color[channel]);
		}
	}

	material->ambient = data.get("ambient").asFloat(material->ambient);
	material->smoothness = data.get("smoothness").asFloat(material->smoothness);
	material->shininess = data.get("shininess").asFloat(material->shininess);
	material->textureScale = data.get("textureScale").asFloat(material->textureScale);
	material->cullBack = data.get("cullBack").asBool(material->cullBack);
	material->clampTexture = data.get("clampTexture").asBool(material->clampTexture);

	std::string texture = data.get("texture").asString();

	if (!texture.empty())
	{
		material->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(toWide(texture).c_str()));
	}

	std::string shader = data.get("shader").asString();

	if (!shader.empty())
	{
		material->setPixelShader(GraphicsEngine::get()->getPixelShader(toWide(shader).c_str(), "main"));
	}

	unsigned int slot = (unsigned int)data.get("slot").asInt(0);

	renderer->setMaterial(slot, material);

	// Слот 0 стає і спільним матеріалом, щоб меші без поділу на частини теж малювалися ним
	if (slot == 0) renderer->setMaterial(material);
}

// Створює один компонент об'єкта за прочитаними даними
static void buildComponent(Entity* entity, const JsonValue& data, const std::vector<Entity*>& created)
{
	std::string type = data.get("type").asString();

	if (type.empty()) return;

	Component* component = nullptr;

	// Рухомість та маса фізичного тіла задаються лише конструктором, тому їх треба знати
	// ще до створення компонента, а не привласнювати потім у deserialize
	if (type == "RigidBody")
	{
		bool isStatic = data.get("static").asBool(true);
		float mass = data.get("mass").asFloat(1.0f);

		component = isStatic ? entity->addComponent<RigidBody>(true) : entity->addComponent<RigidBody>(mass, false);
	}
	else
	{
		component = ComponentRegistry::create(type, entity);
	}

	if (component == nullptr)
	{
		std::cout << "Unknown component type in scene: " << type << std::endl;
		return;
	}

	if (Renderer* renderer = dynamic_cast<Renderer*>(component))
	{
		std::string mesh = data.get("mesh").asString();

		if (!mesh.empty())
		{
			renderer->setMesh(GraphicsEngine::get()->getMeshManager()->createMeshFromFile(toWide(mesh).c_str()));
		}

		renderer->castShadows = data.get("castShadows").asBool(renderer->castShadows);

		const JsonValue& materials = data.get("materials");

		for (size_t slot = 0; slot < materials.size(); slot++)
		{
			buildMaterial(renderer, materials.at(slot));
		}
	}

	SceneReader reader(data, created);
	component->deserialize(reader);
}

// Відновлює сцену з тексту, знищивши те, що було у сцені до цього
void SceneSerializer::deserialize(const std::string& text)
{
	std::string error;

	JsonValue scene = JsonValue::parse(text, &error);

	if (!error.empty())
	{
		std::cout << "Scene is not valid JSON: " << error << std::endl;
		return;
	}

	const JsonValue& parsed = scene.get("entities");

	if (parsed.getType() != JsonValue::Type::Array)
	{
		std::cout << "Scene has no entity list" << std::endl;
		return;
	}

	// Посилання між об'єктами зберігаються номером у цьому списку, тому пропустити зіпсований
	// запис не можна: усі наступні номери зсунулися б. Такий файл відхиляємо цілком
	for (size_t i = 0; i < parsed.size(); i++)
	{
		if (parsed.at(i).getType() != JsonValue::Type::Object)
		{
			std::cout << "Scene entity " << i << " is not an object" << std::endl;
			return;
		}
	}

	// Сцена читається повністю до того, як щось буде знищено: інакше помилка у файлі
	// лишила б редактор із порожнім світом замість попередньої сцени
	EntityManager::get()->onSceneLoadStart();

	// Спершу створюються всі об'єкти, щоб посилання компонентів було на що розв'язувати
	std::vector<Entity*> created;

	for (size_t i = 0; i < parsed.size(); i++)
	{
		const JsonValue& data = parsed.at(i);

		Entity* entity = data.get("prefab").asBool(false) ? new Prefab() : new Entity();

		entity->setName(data.get("name").asString("Entity"));
		entity->isActiveSelf = data.get("active").asBool(true);

		created.push_back(entity);
	}

	// Зв'язки батько-дитина встановлюються після створення всіх об'єктів
	for (size_t i = 0; i < created.size(); i++)
	{
		int parent = parsed.at(i).get("parent").asInt(-1);

		if (parent >= 0 && parent < (int)created.size())
		{
			created[i]->setParent(created[parent]);
		}
	}

	// Тепер відомо, чи має об'єкт батька, тому трансформацію можна застосувати правильно.
	// Зробити це треба до створення компонентів: коллайдер і фізичне тіло будуються за
	// поточним масштабом, тож із одиничним масштабом фізика вийшла б зовсім іншого розміру
	for (size_t i = 0; i < created.size(); i++)
	{
		const JsonValue& data = parsed.at(i).get("transform");

		Vector3 position = jsonToVector(data.get("position"), Vector3(0.0f, 0.0f, 0.0f));
		Vector3 rotation = jsonToVector(data.get("rotation"), Vector3(0.0f, 0.0f, 0.0f));
		Vector3 scale = jsonToVector(data.get("scale"), Vector3(1.0f, 1.0f, 1.0f));

		Transform* transform = created[i]->getTransform();

		if (created[i]->getParent())
		{
			transform->setLocalScale(scale);
			transform->setLocalRotation(rotation);
			transform->setLocalPosition(position);
		}
		else
		{
			transform->setScale(scale);
			transform->setRotation(rotation);
			transform->setPosition(position);
		}
	}

	for (size_t i = 0; i < created.size(); i++)
	{
		const JsonValue& components = parsed.at(i).get("components");

		for (size_t c = 0; c < components.size(); c++)
		{
			buildComponent(created[i], components.at(c), created);
		}
	}

	EntityManager::get()->onSceneLoadFinished();
}

// Зберігає сцену у файл
bool SceneSerializer::saveToFile(const std::string& path)
{
	std::ofstream file(path);

	if (!file.is_open()) return false;

	file << serialize();

	return true;
}

// Завантажує сцену з файлу
bool SceneSerializer::loadFromFile(const std::string& path)
{
	std::ifstream file(path);

	if (!file.is_open()) return false;

	std::ostringstream buffer;
	buffer << file.rdbuf();

	deserialize(buffer.str());

	return true;
}
