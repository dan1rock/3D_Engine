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
#include "GlobalResources.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <iostream>

// Номер формату: змінюється, коли файли старих версій більше не читаються так само
static const int SCENE_VERSION = 4;

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

// Складає опис одного матеріалу
static JsonValue writeMaterial(Material* material)
{
	JsonValue out = JsonValue::object();

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
std::string SceneSerializer::serialize(bool includePersistent)
{
	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	// Індекс кожного об'єкта потрібен, щоб зберегти зв'язки батько-дитина та посилання компонентів
	std::unordered_map<Entity*, int> indices;

	int index = 0;

	for (Entity* entity : entities)
	{
		// Об'єкти, що переживають зміну сцени, не зберігаються: інакше завантаження створить їх копію
		if (entity->dontDestroyOnLoad && !includePersistent) continue;

		indices[entity] = index++;
	}

	// Матеріали пишуться однією таблицею, а рендер-компоненти посилаються на них номером.
	// Інакше спільний матеріал записався б окремо для кожного слота та об'єкта і після
	// завантаження розпався б на копії: правка одного вже не змінювала б решту
	Material* defaultMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	std::unordered_map<Material*, int> materialIndices;
	JsonValue materialList = JsonValue::array();

	// Повертає номер матеріалу в таблиці, а -1 означає спільний матеріал рушія за замовчуванням
	auto materialIndex = [&](Material* material) -> int
	{
		if (material == defaultMaterial) return -1;

		auto it = materialIndices.find(material);

		if (it != materialIndices.end()) return it->second;

		int newIndex = (int)materialList.size();

		materialIndices[material] = newIndex;
		materialList.push(writeMaterial(material));

		return newIndex;
	};

	JsonValue entityList = JsonValue::array();

	for (Entity* entity : entities)
	{
		if (entity->dontDestroyOnLoad && !includePersistent) continue;

		JsonValue entityValue = JsonValue::object();

		entityValue.set("index", indices[entity]);
		entityValue.set("name", entity->getName());
		entityValue.set("active", entity->isActiveSelf);

		// Образ треба відрізнити від звичайного об'єкта, бо інакше він оживе як окремий об'єкт сцени
		// і водночас компоненти, що на нього посилаються, лишаться без образу для створення копій
		if (dynamic_cast<Prefab*>(entity) != nullptr) entityValue.set("prefab", true);

		if (entity->dontDestroyOnLoad) entityValue.set("persistent", true);

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

				// Спільний матеріал і лише ті слоти, яким задано власний, як і в самому компоненті
				if (Material* shared = renderer->getSharedMaterial())
				{
					componentValue.set("material", materialIndex(shared));
				}

				JsonValue slotList = JsonValue::array();

				for (unsigned int slot = 0; slot < renderer->getSlotMaterialCount(); slot++)
				{
					Material* material = renderer->getSlotMaterial(slot);

					if (material == nullptr) continue;

					JsonValue slotValue = JsonValue::object();

					slotValue.set("slot", (int)slot);
					slotValue.set("material", materialIndex(material));

					slotList.push(slotValue);
				}

				if (slotList.size() > 0) componentValue.set("slotMaterials", slotList);
			}

			// Решту полів компонент записує сам, бо лише він знає, що саме варто зберігати
			SceneWriteVisitor writer(componentValue, indices);
			component->visitProperties(writer);

			componentList.push(componentValue);
		}

		if (componentList.size() > 0) entityValue.set("components", componentList);

		entityList.push(entityValue);
	}

	JsonValue scene = JsonValue::object();

	scene.set("version", SCENE_VERSION);
	scene.set("materials", materialList);
	scene.set("entities", entityList);

	return scene.toString();
}

// Створює матеріал за його описом у файлі
static Material* createMaterial(const JsonValue& data)
{
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

	return material;
}

// Повертає матеріал з таблиці за номером; -1 означає спільний матеріал рушія за замовчуванням
static Material* lookupMaterial(int index, const std::vector<Material*>& materials)
{
	if (index == -1) return GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	if (index >= 0 && index < (int)materials.size()) return materials[index];

	return nullptr;
}

// Створює один компонент об'єкта за прочитаними даними
static void buildComponent(Entity* entity, const JsonValue& data, const std::vector<Entity*>& created, const std::vector<Material*>& materials)
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

		// Масу передаємо і нерухомому тілу: setMass її вже не прийме, а без неї збережене
		// значення губилося б і після перемикання на рухоме тіло тут була б одиниця
		component = entity->addComponent<RigidBody>(mass, isStatic);
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

		if (data.has("material"))
		{
			if (Material* shared = lookupMaterial(data.get("material").asInt(-2), materials))
			{
				renderer->setMaterial(shared);
			}
		}

		const JsonValue& slotList = data.get("slotMaterials");

		for (size_t i = 0; i < slotList.size(); i++)
		{
			const JsonValue& slotValue = slotList.at(i);

			Material* material = lookupMaterial(slotValue.get("material").asInt(-2), materials);

			if (material) renderer->setMaterial((unsigned int)slotValue.get("slot").asInt(0), material);
		}

		// Файли третьої версії тримали матеріали прямо в компоненті, окремо для кожного слота
		const JsonValue& legacyMaterials = data.get("materials");

		for (size_t i = 0; i < legacyMaterials.size(); i++)
		{
			const JsonValue& materialValue = legacyMaterials.at(i);

			Material* material = createMaterial(materialValue);
			unsigned int slot = (unsigned int)materialValue.get("slot").asInt(0);

			renderer->setMaterial(slot, material);

			if (slot == 0) renderer->setMaterial(material);
		}
	}

	SceneReadVisitor reader(data, created);
	component->visitProperties(reader);
}

// Відновлює сцену з тексту, знищивши те, що було у сцені до цього
bool SceneSerializer::deserialize(const std::string& text, std::vector<Entity*>* createdOut, bool replacePersistent)
{
	std::string error;

	JsonValue scene = JsonValue::parse(text, &error);

	if (!error.empty())
	{
		std::cout << "Scene is not valid JSON: " << error << std::endl;
		return false;
	}

	const JsonValue& parsed = scene.get("entities");

	if (parsed.getType() != JsonValue::Type::Array)
	{
		std::cout << "Scene has no entity list" << std::endl;
		return false;
	}

	// Посилання між об'єктами зберігаються номером у цьому списку, тому пропустити зіпсований
	// запис не можна: усі наступні номери зсунулися б. Такий файл відхиляємо цілком
	for (size_t i = 0; i < parsed.size(); i++)
	{
		if (parsed.at(i).getType() != JsonValue::Type::Object)
		{
			std::cout << "Scene entity " << i << " is not an object" << std::endl;
			return false;
		}
	}

	// Сцена читається повністю до того, як щось буде знищено: інакше помилка у файлі
	// лишила б редактор із порожнім світом замість попередньої сцени
	if (replacePersistent)
	{
		// Старі екземпляри мають зникнути до створення нових: інакше їх стало б по два, а
		// компоненти-одинаки на кшталт SceneChanger знищили б новий об'єкт просто під час читання
		std::vector<Entity*> persistentRoots;

		for (Entity* entity : EntityManager::get()->getEntities())
		{
			if (entity->dontDestroyOnLoad && entity->getParent() == nullptr) persistentRoots.push_back(entity);
		}

		for (Entity* entity : persistentRoots)
		{
			entity->destroy();
		}
	}

	EntityManager::get()->onSceneLoadStart();

	// Матеріали створюються лише тепер: onSceneLoadStart видаляє всі матеріали сцени,
	// тож створені раніше зникли б разом зі старими
	std::vector<Material*> materials;

	const JsonValue& materialList = scene.get("materials");

	for (size_t i = 0; i < materialList.size(); i++)
	{
		materials.push_back(createMaterial(materialList.at(i)));
	}

	// Спершу створюються всі об'єкти, щоб посилання компонентів було на що розв'язувати
	std::vector<Entity*> created;

	for (size_t i = 0; i < parsed.size(); i++)
	{
		const JsonValue& data = parsed.at(i);

		Entity* entity = data.get("prefab").asBool(false) ? new Prefab() : new Entity();

		entity->setName(data.get("name").asString("Entity"));
		entity->isActiveSelf = data.get("active").asBool(true);
		entity->dontDestroyOnLoad = data.get("persistent").asBool(false);

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
			buildComponent(created[i], components.at(c), created, materials);
		}
	}

	EntityManager::get()->onSceneLoadFinished();

	if (createdOut) *createdOut = created;

	return true;
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

	return deserialize(buffer.str());
}
