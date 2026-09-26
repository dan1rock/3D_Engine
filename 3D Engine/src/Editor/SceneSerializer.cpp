#include "SceneSerializer.h"
#include "SceneIO.h"
#include "Json.h"
#include "PrefabLibrary.h"
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
#include <set>
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
JsonValue SceneSerializer::writeMaterial(Material* material)
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

// Переносить у матеріал поля з опису, замінюючи його текстури
void SceneSerializer::readMaterial(Material* material, const JsonValue& data)
{
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

	// Матеріал можуть оновлювати на місці, тож старі текстури прибираються, а не доповнюються
	while (material->getTextureCount() > 0)
	{
		material->removeTexture(0);
	}

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
}

// Номери об'єктів і спільна таблиця матеріалів, які набираються під час запису
struct WriteContext
{
	std::unordered_map<Entity*, int> indices;
	std::unordered_map<Material*, int> materialIndices;
	JsonValue materials = JsonValue::array();
	Material* defaultMaterial = nullptr;

	// Повертає номер матеріалу в таблиці, а -1 означає спільний матеріал рушія за замовчуванням
	int materialIndex(Material* material)
	{
		if (material == defaultMaterial) return -1;

		auto it = materialIndices.find(material);

		if (it != materialIndices.end()) return it->second;

		int newIndex = (int)materials.size();

		materialIndices[material] = newIndex;
		materials.push(SceneSerializer::writeMaterial(material));

		return newIndex;
	}
};

// Записує один об'єкт. Корінь префаба не має батька в описі, а його позиція й поворот обнуляються:
// у кожного екземпляра вони свої. Лише сцена пише зв'язки з префабами та позначки образів
static JsonValue writeEntity(Entity* entity, WriteContext& context, bool prefabRoot, bool sceneMode)
{
	JsonValue entityValue = JsonValue::object();

	entityValue.set("index", context.indices[entity]);
	entityValue.set("name", entity->getName());
	entityValue.set("active", entity->isActiveSelf);

	if (sceneMode)
	{
		// Образ треба відрізнити від звичайного об'єкта, бо інакше він оживе як окремий об'єкт сцени
		// і водночас компоненти, що на нього посилаються, лишаться без образу для створення копій
		if (dynamic_cast<Prefab*>(entity) != nullptr) entityValue.set("prefab", true);

		if (entity->dontDestroyOnLoad) entityValue.set("persistent", true);

		// Екземпляр зберігає і повні дані, і перелік змінених властивостей: після завантаження решта
		// береться з файлу префаба, а якщо файл зник, об'єкт однаково відновиться з цих даних
		if (!entity->prefabAsset.empty())
		{
			entityValue.set("prefabAsset", entity->prefabAsset);

			JsonValue overrideList = JsonValue::array();

			for (const std::string& key : PrefabLibrary::get()->computeOverrides(entity))
			{
				overrideList.push(key);
			}

			entityValue.set("prefabOverrides", overrideList);
		}
	}

	Entity* parent = entity->getParent();

	// Кореневий об'єкт не має відносно чого зберігати локальні координати, тому пишемо світові
	bool hasParent = parent != nullptr && context.indices.count(parent) > 0;

	entityValue.set("parent", hasParent ? context.indices[parent] : -1);

	Transform* transform = entity->getTransform();

	JsonValue transformValue = JsonValue::object();

	if (prefabRoot)
	{
		// Масштаб беремо у тому просторі, де живе об'єкт: для дочірнього це простір батька
		Vector3 scale = parent ? transform->getLocalScale() : transform->getScale();

		transformValue.set("position", vectorToJson(Vector3(0.0f, 0.0f, 0.0f)));
		transformValue.set("rotation", vectorToJson(Vector3(0.0f, 0.0f, 0.0f)));
		transformValue.set("scale", vectorToJson(scale));
	}
	else
	{
		transformValue.set("position", vectorToJson(hasParent ? transform->getLocalPosition() : transform->getPosition()));
		transformValue.set("rotation", vectorToJson(hasParent ? transform->getLocalRotation() : transform->getRotation()));
		transformValue.set("scale", vectorToJson(hasParent ? transform->getLocalScale() : transform->getScale()));
	}

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
				componentValue.set("material", context.materialIndex(shared));
			}

			JsonValue slotList = JsonValue::array();

			for (unsigned int slot = 0; slot < renderer->getSlotMaterialCount(); slot++)
			{
				Material* material = renderer->getSlotMaterial(slot);

				if (material == nullptr) continue;

				JsonValue slotValue = JsonValue::object();

				slotValue.set("slot", (int)slot);
				slotValue.set("material", context.materialIndex(material));

				slotList.push(slotValue);
			}

			if (slotList.size() > 0) componentValue.set("slotMaterials", slotList);
		}

		// Решту полів компонент записує сам, бо лише він знає, що саме варто зберігати
		SceneWriteVisitor writer(componentValue, context.indices);
		component->visitProperties(writer);

		componentList.push(componentValue);
	}

	if (componentList.size() > 0) entityValue.set("components", componentList);

	return entityValue;
}

// Записує поточну сцену у текст
std::string SceneSerializer::serialize(bool includePersistent)
{
	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	WriteContext context;
	context.defaultMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	// Індекс кожного об'єкта потрібен, щоб зберегти зв'язки батько-дитина та посилання компонентів
	int index = 0;

	for (Entity* entity : entities)
	{
		// Об'єкти, що переживають зміну сцени, не зберігаються: інакше завантаження створить їх копію
		if (entity->dontDestroyOnLoad && !includePersistent) continue;

		context.indices[entity] = index++;
	}

	JsonValue entityList = JsonValue::array();

	for (Entity* entity : entities)
	{
		if (entity->dontDestroyOnLoad && !includePersistent) continue;

		entityList.push(writeEntity(entity, context, false, true));
	}

	// Матеріали пишуться однією таблицею, а рендер-компоненти посилаються на них номером.
	// Інакше спільний матеріал записався б окремо для кожного слота та об'єкта і після
	// завантаження розпався б на копії: правка одного вже не змінювала б решту
	JsonValue scene = JsonValue::object();

	scene.set("version", SCENE_VERSION);
	scene.set("materials", context.materials);
	scene.set("entities", entityList);

	return scene.toString();
}

// Дописує об'єкт і всіх його нащадків у порядку дерева
static void collectSubtree(Entity* entity, std::vector<Entity*>& out)
{
	out.push_back(entity);

	for (Entity* child : *entity->getChildren())
	{
		collectSubtree(child, out);
	}
}

// Записує об'єкт разом з усіма нащадками у тому самому форматі, що й сцену
JsonValue SceneSerializer::serializeSubtree(Entity* root)
{
	std::vector<Entity*> entities;
	collectSubtree(root, entities);

	WriteContext context;
	context.defaultMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	for (size_t i = 0; i < entities.size(); i++)
	{
		context.indices[entities[i]] = (int)i;
	}

	JsonValue entityList = JsonValue::array();

	for (Entity* entity : entities)
	{
		entityList.push(writeEntity(entity, context, entity == root, false));
	}

	JsonValue data = JsonValue::object();

	data.set("version", SCENE_VERSION);
	data.set("materials", context.materials);
	data.set("entities", entityList);

	return data;
}

// Повертає матеріал з таблиці за номером; -1 означає спільний матеріал рушія за замовчуванням
static Material* lookupMaterial(int index, const std::vector<Material*>& materials)
{
	if (index == -1) return GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	if (index >= 0 && index < (int)materials.size()) return materials[index];

	return nullptr;
}

// Створює компонент за описом; самі поля потім задають applyRenderer та applyProperties
Component* SceneSerializer::createComponent(Entity* entity, const JsonValue& data)
{
	std::string type = data.get("type").asString();

	if (type.empty()) return nullptr;

	Component* component = nullptr;

	// Рухомість та маса фізичного тіла задаються лише конструктором, тому їх треба знати
	// ще до створення компонента, а не привласнювати потім
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
	}

	return component;
}

// Переносить у рендер-компонент меш і тіні з опису, якщо вони в ньому є
void SceneSerializer::applyRenderer(Renderer* renderer, const JsonValue& data)
{
	if (data.has("mesh"))
	{
		std::string mesh = data.get("mesh").asString();

		renderer->setMesh(mesh.empty() ? nullptr : GraphicsEngine::get()->getMeshManager()->createMeshFromFile(toWide(mesh).c_str()));
	}

	renderer->castShadows = data.get("castShadows").asBool(renderer->castShadows);
}

// Переносить у компонент його власні поля з опису; посилання на об'єкти шукаються в entities
void SceneSerializer::applyProperties(Component* component, const JsonValue& data, const std::vector<Entity*>& entities)
{
	SceneReadVisitor reader(data, entities);
	component->visitProperties(reader);
}

// Призначає рендер-компоненту матеріали з таблиці, як їх записано в описі
static void assignMaterials(Renderer* renderer, const JsonValue& data, const std::vector<Material*>& materials, bool asTemplate)
{
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

		Material* material = new Material();
		SceneSerializer::readMaterial(material, materialValue);

		material->dontDeleteOnLoad = asTemplate;

		unsigned int slot = (unsigned int)materialValue.get("slot").asInt(0);

		renderer->setMaterial(slot, material);

		if (slot == 0) renderer->setMaterial(material);
	}
}

// Створює об'єкти з опису; кореневі стають дочірніми для parent
static std::vector<Entity*> buildEntities(const JsonValue& data, Entity* parent, bool asTemplate)
{
	const JsonValue& parsed = data.get("entities");

	// Матеріали створюються першими, щоб рендер-компоненти одразу могли на них посилатися.
	// Образ префаба живе між сценами, тож і його матеріали не мають зникати при їх зміні
	std::vector<Material*> materials;

	const JsonValue& materialList = data.get("materials");

	for (size_t i = 0; i < materialList.size(); i++)
	{
		Material* material = new Material();
		SceneSerializer::readMaterial(material, materialList.at(i));

		material->dontDeleteOnLoad = asTemplate;

		materials.push_back(material);
	}

	// Спершу створюються всі об'єкти, щоб посилання компонентів було на що розв'язувати
	std::vector<Entity*> created;

	for (size_t i = 0; i < parsed.size(); i++)
	{
		const JsonValue& entityData = parsed.at(i);

		// Частини образу теж образи: їхні компоненти не мають прокидатися і створювати фізику
		bool makePrefab = asTemplate || entityData.get("prefab").asBool(false);

		Entity* entity = makePrefab ? new Prefab() : new Entity();

		entity->setName(entityData.get("name").asString("Entity"));
		entity->isActiveSelf = entityData.get("active").asBool(true);
		entity->dontDestroyOnLoad = entityData.get("persistent").asBool(false);
		entity->prefabAsset = entityData.get("prefabAsset").asString();

		created.push_back(entity);
	}

	// Корінь образу вимкнений: так увесь образ лишається поза оновленням і малюванням
	if (asTemplate && !created.empty()) created[0]->isActiveSelf = false;

	// Зв'язки батько-дитина встановлюються після створення всіх об'єктів
	for (size_t i = 0; i < created.size(); i++)
	{
		int parentIndex = parsed.at(i).get("parent").asInt(-1);

		if (parentIndex >= 0 && parentIndex < (int)created.size())
		{
			created[i]->setParent(created[parentIndex]);
		}
		else if (parent)
		{
			created[i]->setParent(parent);
		}
	}

	// Тепер відомо, чи має об'єкт батька, тому трансформацію можна застосувати правильно.
	// Зробити це треба до створення компонентів: коллайдер і фізичне тіло будуються за
	// поточним масштабом, тож із одиничним масштабом фізика вийшла б зовсім іншого розміру
	for (size_t i = 0; i < created.size(); i++)
	{
		const JsonValue& transformData = parsed.at(i).get("transform");

		Vector3 position = jsonToVector(transformData.get("position"), Vector3(0.0f, 0.0f, 0.0f));
		Vector3 rotation = jsonToVector(transformData.get("rotation"), Vector3(0.0f, 0.0f, 0.0f));
		Vector3 scale = jsonToVector(transformData.get("scale"), Vector3(1.0f, 1.0f, 1.0f));

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
		// Об'єкт уже знищено прокиданням компонента - його власного або предка
		if (created[i] == nullptr) continue;

		const JsonValue& components = parsed.at(i).get("components");

		for (size_t c = 0; c < components.size(); c++)
		{
			const JsonValue& componentData = components.at(c);

			unsigned int removals = EntityManager::get()->getRemovalCount();

			Component* component = SceneSerializer::createComponent(created[i], componentData);

			// Компонент прокидається вже під час створення і може знищити свій об'єкт, як копія
			// одинака на кшталт SceneChanger, коли такий уже пережив зміну сцени. Тоді разом з
			// об'єктом звільнено і сам компонент, і нащадків, тож їхні вказівники забуваємо
			if (EntityManager::get()->getRemovalCount() != removals)
			{
				for (Entity*& entity : created)
				{
					if (entity && !EntityManager::get()->isAlive(entity)) entity = nullptr;
				}

				if (created[i] == nullptr) break;
			}

			if (component == nullptr) continue;

			if (Renderer* renderer = dynamic_cast<Renderer*>(component))
			{
				SceneSerializer::applyRenderer(renderer, componentData);
				assignMaterials(renderer, componentData, materials, asTemplate);
			}

			SceneSerializer::applyProperties(component, componentData, created);
		}
	}

	// Образ не є частиною сцени: без реєстрації його не видно в дереві, він не потрапляє у файл
	// сцени і не знищується під час її зміни
	if (asTemplate)
	{
		for (Entity* entity : created)
		{
			if (entity) EntityManager::get()->unregisterEntity(entity);
		}
	}

	return created;
}

// Створює об'єкти з опису, не чіпаючи решту сцени
std::vector<Entity*> SceneSerializer::buildSubtree(const JsonValue& data, Entity* parent, bool asTemplate)
{
	return buildEntities(data, parent, asTemplate);
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

	// Матеріали створюються лише всередині: onSceneLoadStart видаляє всі матеріали сцени,
	// тож створені раніше зникли б разом зі старими
	std::vector<Entity*> created = buildEntities(scene, nullptr, false);

	// Екземпляри префабів доганяють свої файли: усе, що в екземплярі не змінювали, береться
	// з префаба, тож правки файлу, зроблені поки сцена була закрита, теж з'являються
	// Корені екземплярів збираються заздалегідь: синхронізація одного може прибрати дочірні
	// об'єкти іншого, тож під час неї до списку створених об'єктів звертатися не можна
	std::vector<std::pair<Entity*, std::set<std::string>>> instances;

	for (size_t i = 0; i < created.size(); i++)
	{
		if (created[i] == nullptr || created[i]->prefabAsset.empty()) continue;

		std::set<std::string> overrides;

		const JsonValue& overrideList = parsed.at(i).get("prefabOverrides");

		for (size_t k = 0; k < overrideList.size(); k++)
		{
			overrides.insert(overrideList.at(k).asString());
		}

		instances.push_back(std::make_pair(created[i], overrides));
	}

	for (const auto& instance : instances)
	{
		if (!EntityManager::get()->isAlive(instance.first)) continue;

		const JsonValue* prefab = PrefabLibrary::get()->getData(instance.first->prefabAsset);

		if (prefab == nullptr)
		{
			std::cout << "Missing prefab asset " << instance.first->prefabAsset << ", keeping the saved copy" << std::endl;
			continue;
		}

		PrefabLibrary::get()->sync(instance.first, *prefab, instance.second, false);
	}

	EntityManager::get()->onSceneLoadFinished();

	// Об'єкти, які синхронізація прибрала, віддаються як порожні, щоб ніхто не звернувся до них
	if (!instances.empty())
	{
		for (Entity*& entity : created)
		{
			if (!EntityManager::get()->isAlive(entity)) entity = nullptr;
		}
	}

	if (createdOut) *createdOut = created;

	return true;
}

// Зберігає сцену у файл разом з об'єктами, що переживають зміну сцени
bool SceneSerializer::saveToFile(const std::string& path)
{
	std::ofstream file(path);

	if (!file.is_open()) return false;

	// Під час гри такий об'єкт з файлу, завантаженого вдруге, стає копією вже наявного; компоненти-
	// одинаки на кшталт SceneChanger прибирають свою копію самі, як DontDestroyOnLoad у Unity
	file << serialize(true);

	return true;
}
