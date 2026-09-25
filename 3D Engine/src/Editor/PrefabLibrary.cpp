#include "PrefabLibrary.h"
#include "SceneSerializer.h"
#include "SceneIO.h"
#include "EntityManager.h"
#include "Entity.h"
#include "Prefab.h"
#include "Component.h"
#include "Renderer.h"
#include "Material.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace filesystem = std::experimental::filesystem;

// Тека, у якій лежать файли префабів
const char* PrefabLibrary::FOLDER = "Assets\\Prefabs";

// Розкладений на окремі властивості опис піддерева: ключ і значення текстом
typedef std::map<std::string, std::string> FlatData;

// Шлях кожного об'єкта опису всередині префаба та номери його дітей
struct TreeInfo
{
	std::vector<std::string> paths;
	std::vector<std::vector<int>> children;
};

// Будує дерево опису. У піддереві батько завжди стоїть раніше за своїх дітей
static TreeInfo buildTree(const JsonValue& data)
{
	const JsonValue& entities = data.get("entities");

	TreeInfo tree;
	tree.paths.resize(entities.size());
	tree.children.resize(entities.size());

	for (size_t i = 0; i < entities.size(); i++)
	{
		int parent = entities.at(i).get("parent").asInt(-1);

		if (parent >= 0 && parent < (int)i)
		{
			tree.paths[i] = tree.paths[parent] + "/" + std::to_string(tree.children[parent].size());
			tree.children[parent].push_back((int)i);
		}
		else
		{
			// Інших коренів у піддереві не буває; окремий шлях не дасть їм злитися з першим
			tree.paths[i] = i == 0 ? "" : "!" + std::to_string(i);
		}
	}

	return tree;
}

// Повертає матеріал з таблиці опису текстом; -1 означає матеріал рушія за замовчуванням
static std::string materialText(const JsonValue& materials, int index)
{
	if (index == -1) return "default";

	return materials.at((size_t)(index < 0 ? materials.size() : index)).toString();
}

// Повертає значення поля компонента текстом. Номери матеріалів у різних описах різні,
// тому для порівняння замість номера підставляється сам матеріал
static std::string fieldText(const std::string& key, const JsonValue& value, const JsonValue& materials)
{
	if (key == "material") return materialText(materials, value.asInt(-2));

	if (key == "slotMaterials")
	{
		std::string text;

		for (size_t i = 0; i < value.size(); i++)
		{
			const JsonValue& slot = value.at(i);

			text += std::to_string(slot.get("slot").asInt(0)) + ":" + materialText(materials, slot.get("material").asInt(-2)) + ";";
		}

		return text;
	}

	return value.toString();
}

// Розкладає опис піддерева на окремі властивості, щоб два описи можна було порівняти поштучно
static FlatData flatten(const JsonValue& data)
{
	FlatData flat;

	const JsonValue& entities = data.get("entities");
	const JsonValue& materials = data.get("materials");

	TreeInfo tree = buildTree(data);

	for (size_t i = 0; i < entities.size(); i++)
	{
		const JsonValue& entity = entities.at(i);
		const std::string& path = tree.paths[i];

		// Ім'я кореня в кожного екземпляра своє, як в Unity: інакше застосування змін з одного
		// екземпляра перейменувало б усі інші, яких ніхто не перейменовував
		if (i != 0) flat[PrefabLibrary::entityKey(path, "name")] = entity.get("name").toString();

		flat[PrefabLibrary::entityKey(path, "active")] = entity.get("active").toString();

		const JsonValue& transform = entity.get("transform");

		flat[PrefabLibrary::entityKey(path, "scale")] = transform.get("scale").toString();

		// Позиція й поворот кореня в кожного екземпляра свої, тож у порівнянні їх немає зовсім
		if (i != 0)
		{
			flat[PrefabLibrary::entityKey(path, "position")] = transform.get("position").toString();
			flat[PrefabLibrary::entityKey(path, "rotation")] = transform.get("rotation").toString();
		}

		flat[PrefabLibrary::entityKey(path, "children")] = std::to_string(tree.children[i].size());

		const JsonValue& components = entity.get("components");

		std::string types;
		std::map<std::string, int> occurrences;

		for (size_t c = 0; c < components.size(); c++)
		{
			const JsonValue& component = components.at(c);

			std::string type = component.get("type").asString();
			int occurrence = occurrences[type]++;

			types += type + ",";

			for (size_t f = 0; f < component.size(); f++)
			{
				const std::string& key = component.keyAt(f);

				if (key == "type") continue;

				flat[PrefabLibrary::componentKey(path, type, occurrence, key)] = fieldText(key, component.valueAt(f), materials);
			}
		}

		flat[PrefabLibrary::entityKey(path, "components")] = types;
	}

	return flat;
}

// Повертає значення за ключем або порожній рядок, якщо такої властивості немає
static std::string lookup(const FlatData& flat, const std::string& key)
{
	auto it = flat.find(key);

	return it == flat.end() ? std::string() : it->second;
}

// Виносить піддерево одного об'єкта в окремий опис: номери об'єктів і матеріалів стискаються,
// а до таблиці потрапляють лише ті матеріали, якими піддерево справді користується
static JsonValue extractSubtree(const JsonValue& data, int index)
{
	const JsonValue& entities = data.get("entities");
	const JsonValue& materials = data.get("materials");

	// Нащадки об'єкта в описі йдуть одразу за ним, тож досить пройти список уперед
	std::map<int, int> entityRemap;
	std::vector<int> members;

	entityRemap[index] = 0;
	members.push_back(index);

	for (size_t j = (size_t)index + 1; j < entities.size(); j++)
	{
		int parent = entities.at(j).get("parent").asInt(-1);

		if (entityRemap.count(parent) == 0) continue;

		entityRemap[(int)j] = (int)members.size();
		members.push_back((int)j);
	}

	std::map<int, int> materialRemap;
	JsonValue materialList = JsonValue::array();

	// Повертає новий номер матеріалу, переносячи його до нової таблиці за потреби
	auto remapMaterial = [&](int oldIndex) -> int
	{
		if (oldIndex < 0 || oldIndex >= (int)materials.size()) return oldIndex;

		auto it = materialRemap.find(oldIndex);

		if (it != materialRemap.end()) return it->second;

		int newIndex = (int)materialList.size();

		materialRemap[oldIndex] = newIndex;
		materialList.push(materials.at((size_t)oldIndex));

		return newIndex;
	};

	JsonValue entityList = JsonValue::array();

	for (int member : members)
	{
		const JsonValue& entity = entities.at((size_t)member);

		JsonValue copy = JsonValue::object();

		for (size_t f = 0; f < entity.size(); f++)
		{
			const std::string& key = entity.keyAt(f);

			if (key == "components" || key == "index" || key == "parent") continue;

			copy.set(key.c_str(), entity.valueAt(f));
		}

		int parent = entity.get("parent").asInt(-1);

		copy.set("index", entityRemap[member]);
		copy.set("parent", member == index ? -1 : entityRemap[parent]);

		const JsonValue& components = entity.get("components");

		JsonValue componentList = JsonValue::array();

		for (size_t c = 0; c < components.size(); c++)
		{
			const JsonValue& component = components.at(c);

			JsonValue componentCopy = JsonValue::object();

			for (size_t f = 0; f < component.size(); f++)
			{
				const std::string& key = component.keyAt(f);
				const JsonValue& value = component.valueAt(f);

				if (key == "material")
				{
					componentCopy.set("material", remapMaterial(value.asInt(-2)));
				}
				else if (key == "slotMaterials")
				{
					JsonValue slots = JsonValue::array();

					for (size_t s = 0; s < value.size(); s++)
					{
						JsonValue slot = JsonValue::object();

						slot.set("slot", value.at(s).get("slot").asInt(0));
						slot.set("material", remapMaterial(value.at(s).get("material").asInt(-2)));

						slots.push(slot);
					}

					componentCopy.set("slotMaterials", slots);
				}
				else
				{
					componentCopy.set(key.c_str(), value);
				}
			}

			componentList.push(componentCopy);
		}

		if (componentList.size() > 0) copy.set("components", componentList);

		entityList.push(copy);
	}

	JsonValue sub = JsonValue::object();

	sub.set("materials", materialList);
	sub.set("entities", entityList);

	return sub;
}

// Шукає компонент вказаного типу з вказаним порядковим номером серед однотипних
static Component* findComponent(Entity* entity, const std::string& type, int occurrence)
{
	int seen = 0;

	for (Component* component : entity->getComponentList())
	{
		if (type != component->getTypeName()) continue;

		if (seen++ == occurrence) return component;
	}

	return nullptr;
}

// Ставить рендер-компоненту матеріал з опису префаба. Наявний матеріал не змінюється, а
// замінюється новим: ним можуть користуватися й інші об'єкти, яких префаб не стосується
static void syncMaterial(Renderer* renderer, int slot, int index, const JsonValue& materials, bool isTemplate)
{
	Material* material = nullptr;

	if (index == -1)
	{
		material = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
	}
	else if (index >= 0 && index < (int)materials.size())
	{
		material = new Material();
		SceneSerializer::readMaterial(material, materials.at((size_t)index));

		// Образ префаба живе між сценами, тож і його матеріал не має зникати при їх зміні
		material->dontDeleteOnLoad = isTemplate;
	}

	if (material == nullptr) return;

	if (slot < 0) renderer->setMaterial(material);
	else renderer->setMaterial((unsigned int)slot, material);
}

// Стан однієї синхронізації: що з чим зіставлено і що довелося створити заново
struct Synchronizer
{
	const JsonValue& data;
	const std::set<std::string>& overrides;
	bool isTemplate;

	TreeInfo tree;
	FlatData asset;
	FlatData instance;

	// Об'єкт екземпляра для кожного номера об'єкта в описі префаба
	std::vector<Entity*> map;

	// Створене синхронізацією отримує всі поля, а не лише ті, що відрізняються
	std::set<Entity*> freshEntities;
	std::set<Component*> freshComponents;

	Synchronizer(const JsonValue& source, const std::set<std::string>& changed, bool templateMode)
		: data(source), overrides(changed), isTemplate(templateMode)
	{
	}

	// Перевіряє, чи треба перенести властивість: вона не змінена в екземплярі і справді відрізняється
	bool differs(const std::string& key) const
	{
		if (overrides.count(key)) return false;

		return lookup(asset, key) != lookup(instance, key);
	}

	// Узгоджує сам об'єкт, склад його компонентів і дітей, а потім і дітей рекурсивно
	void syncStructure(Entity* entity, int index)
	{
		const JsonValue& entities = data.get("entities");
		const JsonValue& entityData = entities.at((size_t)index);
		const std::string& path = tree.paths[(size_t)index];

		map[(size_t)index] = entity;

		if (index != 0 && differs(PrefabLibrary::entityKey(path, "name")))
		{
			entity->setName(entityData.get("name").asString("Entity"));
		}

		// Корінь образу завжди вимкнений: саме це тримає образ поза оновленням і малюванням
		if (differs(PrefabLibrary::entityKey(path, "active")) && !(isTemplate && index == 0))
		{
			entity->isActiveSelf = entityData.get("active").asBool(true);
		}

		const JsonValue& transformData = entityData.get("transform");
		Transform* transform = entity->getTransform();

		if (differs(PrefabLibrary::entityKey(path, "scale")))
		{
			Vector3 scale = jsonToVector(transformData.get("scale"), Vector3(1.0f, 1.0f, 1.0f));

			if (entity->getParent()) transform->setLocalScale(scale);
			else transform->setScale(scale);
		}

		if (index != 0)
		{
			if (differs(PrefabLibrary::entityKey(path, "rotation")))
			{
				transform->setLocalRotation(jsonToVector(transformData.get("rotation"), Vector3(0.0f, 0.0f, 0.0f)));
			}

			if (differs(PrefabLibrary::entityKey(path, "position")))
			{
				transform->setLocalPosition(jsonToVector(transformData.get("position"), Vector3(0.0f, 0.0f, 0.0f)));
			}
		}

		if (differs(PrefabLibrary::entityKey(path, "components")))
		{
			syncComponentList(entity, entityData.get("components"));
		}

		const std::vector<int>& assetChildren = tree.children[(size_t)index];

		if (differs(PrefabLibrary::entityKey(path, "children")))
		{
			std::vector<Entity*> children(entity->getChildren()->begin(), entity->getChildren()->end());

			// Зайві діти прибираються з кінця, як їх і додавали б
			while (children.size() > assetChildren.size())
			{
				children.back()->destroy();
				children.pop_back();
			}

			// Відсутні будуються з опису префаба разом з усіма своїми нащадками
			for (size_t k = children.size(); k < assetChildren.size(); k++)
			{
				JsonValue sub = extractSubtree(data, assetChildren[k]);

				std::vector<Entity*> built = SceneSerializer::buildSubtree(sub, entity, isTemplate);

				// Для образу побудова вимикає корінь, але тут це лише дитина: їй лишається свій стан
				if (!built.empty()) built[0]->isActiveSelf = sub.get("entities").at(0).get("active").asBool(true);

				for (Entity* part : built)
				{
					freshEntities.insert(part);
				}
			}
		}

		std::vector<Entity*> children(entity->getChildren()->begin(), entity->getChildren()->end());

		for (size_t k = 0; k < children.size() && k < assetChildren.size(); k++)
		{
			syncStructure(children[k], assetChildren[k]);
		}
	}

	// Прибирає компоненти, яких немає в префабі, і додає відсутні; порівнюються типи та їх кількість
	void syncComponentList(Entity* entity, const JsonValue& components)
	{
		std::map<std::string, int> wanted;

		for (size_t c = 0; c < components.size(); c++)
		{
			wanted[components.at(c).get("type").asString()]++;
		}

		// Копія списку, бо видалення його змінює
		std::vector<Component*> existing(entity->getComponentList().begin(), entity->getComponentList().end());
		std::map<std::string, int> seen;

		for (Component* component : existing)
		{
			std::string type = component->getTypeName();

			if (seen[type]++ >= wanted[type]) entity->removeComponent(component);
		}

		std::map<std::string, int> have;

		for (Component* component : entity->getComponentList())
		{
			have[component->getTypeName()]++;
		}

		std::map<std::string, int> occurrences;

		for (size_t c = 0; c < components.size(); c++)
		{
			const JsonValue& componentData = components.at(c);

			std::string type = componentData.get("type").asString();

			if (occurrences[type]++ < have[type]) continue;

			if (Component* created = SceneSerializer::createComponent(entity, componentData))
			{
				freshComponents.insert(created);
			}
		}
	}

	// Переносить поля компонентів: лише ті, що відрізняються і не змінені в екземплярі
	void syncFields()
	{
		const JsonValue& entities = data.get("entities");
		const JsonValue& materials = data.get("materials");

		for (size_t i = 0; i < entities.size(); i++)
		{
			Entity* entity = map[i];

			if (entity == nullptr) continue;

			const std::string& path = tree.paths[i];
			const JsonValue& components = entities.at(i).get("components");

			bool freshEntity = freshEntities.count(entity) > 0;

			std::map<std::string, int> occurrences;

			for (size_t c = 0; c < components.size(); c++)
			{
				const JsonValue& componentData = components.at(c);

				std::string type = componentData.get("type").asString();
				int occurrence = occurrences[type]++;

				Component* component = findComponent(entity, type, occurrence);

				if (component == nullptr) continue;

				bool applyAll = freshEntity || freshComponents.count(component) > 0;

				JsonValue partial = JsonValue::object();

				for (size_t f = 0; f < componentData.size(); f++)
				{
					const std::string& key = componentData.keyAt(f);

					if (key == "type") continue;

					if (!applyAll && !differs(PrefabLibrary::componentKey(path, type, occurrence, key))) continue;

					partial.set(key.c_str(), componentData.valueAt(f));
				}

				if (partial.size() == 0) continue;

				if (Renderer* renderer = dynamic_cast<Renderer*>(component))
				{
					SceneSerializer::applyRenderer(renderer, partial);

					if (partial.has("material"))
					{
						syncMaterial(renderer, -1, partial.get("material").asInt(-2), materials, isTemplate);
					}

					const JsonValue& slots = partial.get("slotMaterials");

					for (size_t s = 0; s < slots.size(); s++)
					{
						syncMaterial(renderer, slots.at(s).get("slot").asInt(0), slots.at(s).get("material").asInt(-2), materials, isTemplate);
					}
				}

				// Посилання в описі префаба — номери його власних об'єктів, тож шукаються вони
				// серед зіставлених об'єктів екземпляра
				SceneSerializer::applyProperties(component, partial, map);
			}
		}
	}
};

// Повертає єдиний екземпляр бібліотеки (синглтон)
PrefabLibrary* PrefabLibrary::get()
{
	static PrefabLibrary instance;
	return &instance;
}

PrefabLibrary::PrefabLibrary()
{
	refresh();
}

// Перечитує перелік файлів префабів у теці
void PrefabLibrary::refresh()
{
	mPaths.clear();

	std::error_code error;

	if (!filesystem::exists(FOLDER, error)) return;

	for (const auto& entry : filesystem::directory_iterator(FOLDER, error))
	{
		if (!filesystem::is_regular_file(entry.path(), error)) continue;
		if (entry.path().extension().string() != ".prefab") continue;

		mPaths.push_back(std::string(FOLDER) + "\\" + entry.path().filename().string());
	}

	std::sort(mPaths.begin(), mPaths.end());
}

// Повертає шляхи всіх знайдених префабів
const std::vector<std::string>& PrefabLibrary::getPaths() const
{
	return mPaths;
}

// Повертає ім'я префаба без теки й розширення
std::string PrefabLibrary::getName(const std::string& path)
{
	return filesystem::path(path).stem().string();
}

// Повертає вміст префаба з файлу, або nullptr, якщо файлу немає чи він зіпсований
const JsonValue* PrefabLibrary::getData(const std::string& path)
{
	auto cached = mData.find(path);

	if (cached != mData.end()) return &cached->second;

	std::ifstream file(path);

	if (!file.is_open()) return nullptr;

	std::ostringstream buffer;
	buffer << file.rdbuf();

	std::string error;
	JsonValue data = JsonValue::parse(buffer.str(), &error);

	if (!error.empty() || data.get("entities").size() == 0)
	{
		std::cout << "Prefab " << path << " is not valid: " << error << std::endl;
		return nullptr;
	}

	return &(mData[path] = data);
}

// Записує вміст префаба у файл
bool PrefabLibrary::writeFile(const std::string& path, const JsonValue& data)
{
	std::ofstream file(path);

	if (!file.is_open())
	{
		std::cout << "Failed to write prefab " << path << std::endl;
		return false;
	}

	file << data.toString();

	return true;
}

// Повертає прихований образ префаба, на який посилаються поля компонентів гри
Prefab* PrefabLibrary::getTemplate(const std::string& path)
{
	auto existing = mTemplates.find(path);

	if (existing != mTemplates.end()) return existing->second;

	if (mBuilding.count(path))
	{
		std::cout << "Prefab " << path << " references itself" << std::endl;
		return nullptr;
	}

	const JsonValue* data = getData(path);

	if (data == nullptr) return nullptr;

	mBuilding.insert(path);
	std::vector<Entity*> built = SceneSerializer::buildSubtree(*data, nullptr, true);
	mBuilding.erase(path);

	if (built.empty()) return nullptr;

	Prefab* prefab = dynamic_cast<Prefab*>(built[0]);

	prefab->setName(getName(path));

	mTemplates[path] = prefab;

	return prefab;
}

// Повертає шлях префаба, якщо об'єкт — його образ, інакше порожній рядок
std::string PrefabLibrary::getTemplatePath(Entity* entity) const
{
	for (const auto& entry : mTemplates)
	{
		if (entry.second == entity) return entry.first;
	}

	return std::string();
}

// Зберігає об'єкт разом з нащадками як новий префаб і робить сам об'єкт його екземпляром
std::string PrefabLibrary::createAsset(Entity* root)
{
	std::error_code error;
	filesystem::create_directories(FOLDER, error);

	// Ім'я файлу береться з імені об'єкта без символів, які Windows у файлах не дозволяє
	std::string name = root->getName();

	for (char& symbol : name)
	{
		if (std::string("\\/:*?\"<>|").find(symbol) != std::string::npos) symbol = '_';
	}

	if (name.empty()) name = "Prefab";

	std::string path = std::string(FOLDER) + "\\" + name + ".prefab";

	for (int number = 1; filesystem::exists(path, error); number++)
	{
		path = std::string(FOLDER) + "\\" + name + " " + std::to_string(number) + ".prefab";
	}

	// Вкладених префабів немає: екземпляри всередині стають звичайною частиною нового префаба,
	// інакше їх одночасно тягнули б до себе два різні префаби
	std::vector<Entity*> stack(1, root);

	while (!stack.empty())
	{
		Entity* entity = stack.back();
		stack.pop_back();

		entity->prefabAsset.clear();

		for (Entity* child : *entity->getChildren())
		{
			stack.push_back(child);
		}
	}

	JsonValue data = SceneSerializer::serializeSubtree(root);

	if (!writeFile(path, data)) return std::string();

	mData[path] = data;
	root->prefabAsset = path;

	refresh();

	return path;
}

// Створює у сцені новий екземпляр префаба; parent може бути nullptr
Entity* PrefabLibrary::instantiate(const std::string& path, Entity* parent)
{
	const JsonValue* data = getData(path);

	if (data == nullptr) return nullptr;

	std::vector<Entity*> built = SceneSerializer::buildSubtree(*data, parent, false);

	if (built.empty()) return nullptr;

	built[0]->prefabAsset = path;

	// Новий екземпляр називається так само, як файл префаба
	built[0]->setName(getName(path));

	// Новий об'єкт стає на своє місце в загальному порядку сцени одразу після батька
	EntityManager::get()->sortByHierarchy();

	return built[0];
}

// Повертає корінь екземпляра, до якого належить об'єкт, або nullptr
Entity* PrefabLibrary::findInstanceRoot(Entity* entity)
{
	for (Entity* current = entity; current; current = current->getParent())
	{
		if (!current->prefabAsset.empty()) return current;
	}

	return nullptr;
}

// Повертає ключі властивостей, якими екземпляр відрізняється від свого префаба
std::set<std::string> PrefabLibrary::computeOverrides(Entity* instanceRoot)
{
	std::set<std::string> overrides;

	const JsonValue* data = getData(instanceRoot->prefabAsset);

	if (data == nullptr) return overrides;

	FlatData instance = flatten(SceneSerializer::serializeSubtree(instanceRoot));
	FlatData asset = flatten(*data);

	for (const auto& entry : instance)
	{
		if (lookup(asset, entry.first) != entry.second) overrides.insert(entry.first);
	}

	// Властивість, якої в екземплярі немає зовсім, теж є зміною: наприклад, прибраний компонент
	for (const auto& entry : asset)
	{
		if (instance.count(entry.first) == 0) overrides.insert(entry.first);
	}

	return overrides;
}

// Приводить об'єкт до опису префаба на місці, не чіпаючи перелічених змінених властивостей
void PrefabLibrary::sync(Entity* root, const JsonValue& data, const std::set<std::string>& overrides, bool isTemplate)
{
	if (data.get("entities").size() == 0) return;

	Synchronizer synchronizer(data, overrides, isTemplate);

	synchronizer.tree = buildTree(data);
	synchronizer.asset = flatten(data);
	synchronizer.instance = flatten(SceneSerializer::serializeSubtree(root));
	synchronizer.map.assign(data.get("entities").size(), nullptr);

	// Спершу будова: об'єкти, компоненти і діти, а заодно відповідність між описом та екземпляром
	synchronizer.syncStructure(root, 0);

	// Будова могла змінитися, тож поля порівнюються вже з тим, що вийшло
	synchronizer.instance = flatten(SceneSerializer::serializeSubtree(root));
	synchronizer.syncFields();

	// Нові діти мають стати у загальному порядку сцени одразу за своїм батьком
	if (!isTemplate) EntityManager::get()->sortByHierarchy();
}

// Записує стан екземпляра у префаб і поширює зміну на інші екземпляри та образ
void PrefabLibrary::apply(Entity* instanceRoot)
{
	std::string path = instanceRoot->prefabAsset;

	if (path.empty()) return;

	// Зміни інших екземплярів рахуються ще відносно старого вмісту префаба: після запису файлу
	// вже не можна було б відрізнити їхні власні правки від щойно застосованих
	std::vector<std::pair<Entity*, std::set<std::string>>> others;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		if (entity != instanceRoot && entity->prefabAsset == path)
		{
			others.push_back(std::make_pair(entity, computeOverrides(entity)));
		}
	}

	JsonValue data = SceneSerializer::serializeSubtree(instanceRoot);

	if (!writeFile(path, data)) return;

	mData[path] = data;

	for (const auto& other : others)
	{
		if (EntityManager::get()->isAlive(other.first)) sync(other.first, data, other.second, false);
	}

	// Образ, яким користуються поля компонентів гри, теж має стати новим
	auto templateEntry = mTemplates.find(path);

	if (templateEntry != mTemplates.end()) sync(templateEntry->second, data, std::set<std::string>(), true);
}

// Повертає екземпляру стан префаба, крім позиції й повороту кореня
void PrefabLibrary::revert(Entity* instanceRoot)
{
	const JsonValue* data = getData(instanceRoot->prefabAsset);

	if (data == nullptr) return;

	sync(instanceRoot, *data, std::set<std::string>(), false);
}

// Розриває зв'язок екземпляра з префабом, лишаючи сам об'єкт як є
void PrefabLibrary::unpack(Entity* instanceRoot)
{
	instanceRoot->prefabAsset.clear();
}

// Ключ властивості об'єкта; path — шлях об'єкта всередині префаба
std::string PrefabLibrary::entityKey(const std::string& path, const char* field)
{
	return path + "|" + field;
}

// Ключ поля компонента, що визначається типом і порядковим номером серед однотипних
std::string PrefabLibrary::componentKey(const std::string& path, const std::string& type, int occurrence, const std::string& field)
{
	return path + "|" + type + "#" + std::to_string(occurrence) + "." + field;
}

// Шлях об'єкта всередині екземпляра: номери дочірніх від кореня, як-от "/0/2"
std::string PrefabLibrary::pathInInstance(Entity* entity, Entity* instanceRoot)
{
	std::string path;

	for (Entity* current = entity; current && current != instanceRoot; current = current->getParent())
	{
		Entity* parent = current->getParent();

		if (parent == nullptr) break;

		int ordinal = 0;

		for (Entity* sibling : *parent->getChildren())
		{
			if (sibling == current) break;
			ordinal++;
		}

		path = "/" + std::to_string(ordinal) + path;
	}

	return path;
}

// Порядковий номер компонента серед компонентів того самого типу в його об'єкті
int PrefabLibrary::componentOccurrence(Component* component)
{
	int occurrence = 0;

	for (Component* other : component->getOwner()->getComponentList())
	{
		if (other == component) break;

		if (std::string(other->getTypeName()) == component->getTypeName()) occurrence++;
	}

	return occurrence;
}
