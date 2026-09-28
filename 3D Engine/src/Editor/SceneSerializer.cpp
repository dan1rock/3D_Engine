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
#include "Texture.h"
#include "DirectionalLight.h"
#include "RigidBody.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "MaterialLibrary.h"

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <set>
#include <iostream>

// Номер формату: змінюється, коли файли старих версій більше не читаються так само
static const int SCENE_VERSION = MaterialLibrary::REFERENCE_VERSION;

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

// Назви режимів матеріалу у файлі; збігаються з назвами в інспекторі
static const char* WORKFLOW_NAMES[] = { "Metallic", "Specular" };
static const char* SURFACE_NAMES[] = { "Opaque", "Transparent" };
static const char* BLEND_NAMES[] = { "Alpha", "Premultiply", "Additive", "Multiply" };
static const char* FACE_NAMES[] = { "Front", "Back", "Both" };
static const char* SMOOTHNESS_SOURCE_NAMES[] = { "MetallicAlpha", "AlbedoAlpha" };

// Ключі карт матеріалу у файлі в порядку MaterialMap
static const char* MAP_KEYS[] = { "baseMap", "metallicMap", "specularMap", "normalMap", "heightMap", "occlusionMap", "emissionMap", "detailMask", "detailAlbedoMap", "detailNormalMap" };

// Повертає номер назви режиму, або запасний, якщо назву не впізнано
static int indexOfName(const JsonValue& value, const char* const* names, int count, int fallback)
{
	std::string name = value.asString();

	for (int i = 0; i < count; i++)
	{
		if (name == names[i]) return i;
	}

	return fallback;
}

// Записує кілька чисел масивом в один рядок
static JsonValue floatsToJson(const float* values, int count)
{
	JsonValue out = JsonValue::array();

	for (int i = 0; i < count; i++)
	{
		out.push(values[i]);
	}

	return out;
}

// Читає масив чисел, лишаючи поточні значення там, де їх немає
static void jsonToFloats(const JsonValue& value, float* values, int count)
{
	if (value.getType() != JsonValue::Type::Array) return;

	for (int i = 0; i < count && (size_t)i < value.size(); i++)
	{
		values[i] = value.at((size_t)i).asFloat(values[i]);
	}
}

// Складає опис одного матеріалу
JsonValue SceneSerializer::writeMaterial(Material* material)
{
	const MaterialProperties& p = material->properties;

	JsonValue out = JsonValue::object();

	out.set("workflow", WORKFLOW_NAMES[(int)p.workflow]);
	out.set("surface", SURFACE_NAMES[(int)p.surface]);
	out.set("blend", BLEND_NAMES[(int)p.blend]);
	out.set("renderFace", FACE_NAMES[(int)p.renderFace]);
	out.set("alphaClipping", p.alphaClipping);
	out.set("alphaCutoff", p.alphaCutoff);
	out.set("receiveShadows", p.receiveShadows);

	out.set("baseColor", floatsToJson(p.baseColor, 4));
	out.set("metallic", p.metallic);
	out.set("smoothness", p.smoothness);
	out.set("smoothnessSource", SMOOTHNESS_SOURCE_NAMES[(int)p.smoothnessSource]);
	out.set("specularColor", floatsToJson(p.specularColor, 3));
	out.set("normalScale", p.normalScale);
	out.set("heightScale", p.heightScale);
	out.set("occlusionStrength", p.occlusionStrength);
	out.set("emission", p.emission);
	out.set("emissionColor", floatsToJson(p.emissionColor, 3));
	out.set("tiling", floatsToJson(p.tiling, 2));
	out.set("offset", floatsToJson(p.offset, 2));

	out.set("detailAlbedoScale", p.detailAlbedoScale);
	out.set("detailNormalScale", p.detailNormalScale);
	out.set("detailTiling", floatsToJson(p.detailTiling, 2));
	out.set("detailOffset", floatsToJson(p.detailOffset, 2));

	out.set("specularHighlights", p.specularHighlights);
	out.set("environmentReflections", p.environmentReflections);
	out.set("sortingPriority", p.sortingPriority);

	// Карти пишуться шляхами файлів; відсутня карта в описі не з'являється
	for (int i = 0; i < (int)MaterialMap::Count; i++)
	{
		Texture* texture = material->getMap((MaterialMap)i);

		if (texture) out.set(MAP_KEYS[i], toRelativePath(texture->getFullPath()));
	}

	// Нестандартний піксельний шейдер треба зберегти: інакше матеріал відновиться зі звичайним
	std::wstring shaderPath = material->getPixelShaderPath();

	if (!shaderPath.empty()) out.set("shader", toRelativePath(shaderPath));

	return out;
}

// Переносить у матеріал поля з опису, замінюючи всі його налаштування й карти
void SceneSerializer::readMaterial(Material* material, const JsonValue& data)
{
	// Відсутнє в описі поле отримує типове значення, а не лишається від попереднього стану
	MaterialProperties p;

	p.workflow = (MaterialWorkflow)indexOfName(data.get("workflow"), WORKFLOW_NAMES, 2, 0);
	p.surface = (SurfaceType)indexOfName(data.get("surface"), SURFACE_NAMES, 2, 0);
	p.blend = (BlendMode)indexOfName(data.get("blend"), BLEND_NAMES, 4, 0);
	p.renderFace = (RenderFace)indexOfName(data.get("renderFace"), FACE_NAMES, 3, 0);
	p.alphaClipping = data.get("alphaClipping").asBool(p.alphaClipping);
	p.alphaCutoff = data.get("alphaCutoff").asFloat(p.alphaCutoff);
	p.receiveShadows = data.get("receiveShadows").asBool(p.receiveShadows);

	jsonToFloats(data.get("baseColor"), p.baseColor, 4);
	p.metallic = data.get("metallic").asFloat(p.metallic);
	p.smoothness = data.get("smoothness").asFloat(p.smoothness);
	p.smoothnessSource = (SmoothnessSource)indexOfName(data.get("smoothnessSource"), SMOOTHNESS_SOURCE_NAMES, 2, 0);
	jsonToFloats(data.get("specularColor"), p.specularColor, 3);
	p.normalScale = data.get("normalScale").asFloat(p.normalScale);
	p.heightScale = data.get("heightScale").asFloat(p.heightScale);
	p.occlusionStrength = data.get("occlusionStrength").asFloat(p.occlusionStrength);
	p.emission = data.get("emission").asBool(p.emission);
	jsonToFloats(data.get("emissionColor"), p.emissionColor, 3);
	jsonToFloats(data.get("tiling"), p.tiling, 2);
	jsonToFloats(data.get("offset"), p.offset, 2);

	p.detailAlbedoScale = data.get("detailAlbedoScale").asFloat(p.detailAlbedoScale);
	p.detailNormalScale = data.get("detailNormalScale").asFloat(p.detailNormalScale);
	jsonToFloats(data.get("detailTiling"), p.detailTiling, 2);
	jsonToFloats(data.get("detailOffset"), p.detailOffset, 2);

	p.specularHighlights = data.get("specularHighlights").asBool(p.specularHighlights);
	p.environmentReflections = data.get("environmentReflections").asBool(p.environmentReflections);
	p.sortingPriority = data.get("sortingPriority").asInt(p.sortingPriority);

	// Старі файли мали один колір, одну текстуру з масштабом і прапорець відкидання граней
	if (!data.has("baseColor"))
	{
		jsonToFloats(data.get("color"), p.baseColor, 4);

		float scale = data.get("textureScale").asFloat(1.0f);
		p.tiling[0] = scale;
		p.tiling[1] = scale;

		if (!data.get("cullBack").asBool(true)) p.renderFace = RenderFace::Back;
	}

	material->properties = p;

	for (int i = 0; i < (int)MaterialMap::Count; i++)
	{
		std::string path = data.get(MAP_KEYS[i]).asString();

		// Стара єдина текстура стає основною картою
		if (i == (int)MaterialMap::Base && path.empty()) path = data.get("texture").asString();

		material->setMap((MaterialMap)i, path.empty() ? nullptr : GraphicsEngine::get()->getTextureManager()->createTextureFromFile(toWide(path).c_str()));
	}

	std::string shader = data.get("shader").asString();

	if (!shader.empty())
	{
		material->setPixelShader(GraphicsEngine::get()->getPixelShader(toWide(shader).c_str(), "main"));
	}
}

// Номери об'єктів, які набираються під час запису, та реєстр матеріалів без файлу для історії змін
struct WriteContext
{
	std::unordered_map<Entity*, int> indices;
	MaterialRegistry* registry = nullptr;
	std::vector<Material*> embedded;

	// Повертає посилання на матеріал: DefaultMaterial, шлях файлу, або сам матеріал без файлу; для історії змін - номер у реєстрі
	JsonValue materialReference(Material* material)
	{
		std::string reference = MaterialLibrary::referenceOf(material);

		if (!reference.empty()) return JsonValue(reference);

		if (registry == nullptr) return SceneSerializer::writeMaterial(material);

		embedded.push_back(material);

		JsonValue out = JsonValue::object();
		out.set("embedded", registry->idOf(material));

		return out;
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

			// Спільний матеріал кожного слота; копії режиму гри не записуються
			JsonValue materialList = JsonValue::array();

			for (unsigned int slot = 0; slot < renderer->getMaterialCount(); slot++)
			{
				materialList.push(context.materialReference(renderer->getSharedMaterial(slot)));
			}

			componentValue.set("materials", materialList);
		}

		// Решту полів компонент записує сам, бо лише він знає, що саме варто зберігати
		SceneWriteVisitor writer(componentValue, context.indices);
		component->visitProperties(writer);

		componentList.push(componentValue);
	}

	if (componentList.size() > 0) entityValue.set("components", componentList);

	return entityValue;
}

// Повертає номер матеріалу, за потреби видаючи новий
int MaterialRegistry::idOf(Material* material)
{
	auto it = ids.find(material);

	if (it != ids.end()) return it->second;

	int id = (int)materials.size();

	materials.push_back(material);
	ids[material] = id;

	return id;
}

// Дописує номер об'єкта і номери його нащадків у порядку дерева
static void appendOrder(Entity* entity, std::vector<int>& order)
{
	order.push_back((int)entity->getId());

	for (Entity* child : *entity->getChildren())
	{
		appendOrder(child, order);
	}
}

// Записує стан сцени для історії змін: посилання й батьки як номери об'єктів, матеріали як номери реєстру
void SceneSerializer::captureRecords(SceneRecords& out, MaterialRegistry& registry)
{
	out.entities.clear();
	out.materials.clear();
	out.order.clear();

	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	WriteContext context;
	context.registry = &registry;

	// Посилання й батьки пишуться незмінними номерами об'єктів, а не місцем у переліку
	for (Entity* entity : entities)
	{
		context.indices[entity] = (int)entity->getId();
	}

	for (Entity* entity : entities)
	{
		out.entities[(int)entity->getId()] = writeEntity(entity, context, false, true).toString();
	}

	// Значення матеріалів з файлів теж входять у крок, тож правку матеріалу можна скасувати
	for (Material* material : MaterialLibrary::get()->getLoaded())
	{
		out.materials[registry.idOf(material)] = writeMaterial(material).toString();
	}

	for (Material* material : context.embedded)
	{
		out.materials[registry.idOf(material)] = writeMaterial(material).toString();
	}

	for (Entity* entity : entities)
	{
		if (entity->getParent() == nullptr) appendOrder(entity, out.order);
	}
}

// Приводить компоненти об'єкта до запису; ті, що збігаються за типом і місцем, лишаються тими самими
static void syncRecordComponents(Entity* entity, const JsonValue& components, MaterialRegistry& registry)
{
	std::vector<Component*> existing(entity->getComponentList().begin(), entity->getComponentList().end());

	size_t keep = 0;

	while (keep < existing.size() && keep < components.size() && components.at(keep).get("type").asString() == existing[keep]->getTypeName())
	{
		keep++;
	}

	for (size_t i = existing.size(); i > keep; i--)
	{
		entity->removeComponent(existing[i - 1]);
	}

	std::vector<Component*> current(existing.begin(), existing.begin() + keep);

	unsigned int removals = EntityManager::get()->getRemovalCount();

	for (size_t i = keep; i < components.size(); i++)
	{
		current.push_back(SceneSerializer::createComponent(entity, components.at(i)));

		// Компонент-одинак міг знищити свій об'єкт ще під час прокидання
		if (EntityManager::get()->getRemovalCount() != removals && !EntityManager::get()->isAlive(entity)) return;
	}

	for (size_t i = 0; i < components.size(); i++)
	{
		Component* component = current[i];

		if (component == nullptr) continue;

		const JsonValue& data = components.at(i);

		if (Renderer* renderer = dynamic_cast<Renderer*>(component))
		{
			SceneSerializer::applyRenderer(renderer, data);
			SceneSerializer::applyMaterials(renderer, data, false, &registry);
		}

		SceneReadVisitor reader(data);
		component->visitProperties(reader);
	}
}

// Переводить названі об'єкти й матеріали між записами, торкаючись лише того, що в записі змінилося
void SceneSerializer::applyRecords(const std::vector<RecordChange>& entities, const std::vector<std::pair<int, std::string>>& materials, const std::vector<int>& order, MaterialRegistry& registry)
{
	EntityManager* manager = EntityManager::get();

	std::vector<std::pair<int, JsonValue>> targets;
	std::vector<int> removed;

	// Для кожного об'єкта: чи змінилися батько з трансформацією і чи змінилися компоненти
	std::unordered_map<int, bool> transformChanged;
	std::unordered_map<int, bool> componentsChanged;

	for (const RecordChange& change : entities)
	{
		if (change.to.empty())
		{
			removed.push_back(change.id);
			continue;
		}

		JsonValue to = JsonValue::parse(change.to);

		// Новий об'єкт отримує все; наявний - лише ті частини, що різняться між записами
		bool isNew = change.from.empty();
		JsonValue from = isNew ? JsonValue() : JsonValue::parse(change.from);

		transformChanged[change.id] = isNew || from.get("parent").toString() != to.get("parent").toString() || from.get("transform").toString() != to.get("transform").toString();
		componentsChanged[change.id] = isNew || from.get("components").toString() != to.get("components").toString();

		targets.push_back(std::make_pair(change.id, to));
	}

	// Відсутні об'єкти створюються під тими самими номерами, тож посилання на них знову знаходяться
	for (const auto& target : targets)
	{
		if (manager->findById((unsigned int)target.first)) continue;

		Entity* entity = target.second.get("prefab").asBool(false) ? new Prefab() : new Entity();
		entity->setId((unsigned int)target.first);
	}

	// Імена, прапорці та батьки
	for (const auto& target : targets)
	{
		Entity* entity = manager->findById((unsigned int)target.first);
		const JsonValue& data = target.second;

		entity->setName(data.get("name").asString("Entity"));
		entity->isActiveSelf = data.get("active").asBool(true);
		entity->dontDestroyOnLoad = data.get("persistent").asBool(false);
		entity->prefabAsset = data.get("prefabAsset").asString();

		int parentId = data.get("parent").asInt(-1);
		Entity* parent = parentId >= 0 ? manager->findById((unsigned int)parentId) : nullptr;

		if (entity->getParent() != parent) entity->setParent(parent);
	}

	// Трансформації після батьків, бо дочірній зберігає локальні
	for (const auto& target : targets)
	{
		// Незмінена трансформація не чіпається, тож фізичне тіло не переставляється даремно
		Entity* entity = manager->findById((unsigned int)target.first);

		if (entity == nullptr || !transformChanged[target.first]) continue;

		const JsonValue& transformData = target.second.get("transform");

		Vector3 position = jsonToVector(transformData.get("position"), Vector3(0.0f, 0.0f, 0.0f));
		Vector3 rotation = jsonToVector(transformData.get("rotation"), Vector3(0.0f, 0.0f, 0.0f));
		Vector3 scale = jsonToVector(transformData.get("scale"), Vector3(1.0f, 1.0f, 1.0f));

		Transform* transform = entity->getTransform();

		if (entity->getParent())
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

	// Компоненти після трансформацій: фізика будує форми за поточним масштабом
	for (const auto& target : targets)
	{
		Entity* entity = manager->findById((unsigned int)target.first);

		// Незмінені компоненти лишаються як є, і ресурси їхніх мешів навіть не шукаються
		if (entity && componentsChanged[target.first]) syncRecordComponents(entity, target.second.get("components"), registry);
	}

	// Значення матеріалів правляться на місці, тож ті, хто ними користується, лишаються з ними
	for (const auto& change : materials)
	{
		if (change.first < 0 || change.first >= (int)registry.materials.size()) continue;

		readMaterial(registry.materials[change.first], JsonValue::parse(change.second));
	}

	// Знищення наприкінці: дочірні, що мають лишитися, уже перейшли до інших батьків
	for (int id : removed)
	{
		if (Entity* entity = manager->findById((unsigned int)id)) entity->destroy();
	}

	// Кожен стає в кінець своїх сусідів у порядку запису, тож порядок дерева повторює запис
	if (!order.empty())
	{
		for (int id : order)
		{
			Entity* entity = manager->findById((unsigned int)id);

			if (entity == nullptr) continue;

			if (entity->getParent()) entity->getParent()->moveChildBefore(entity, nullptr);
			else manager->moveRootBefore(entity, nullptr);
		}

		manager->sortByHierarchy();
	}
}

// Записує поточну сцену у текст
std::string SceneSerializer::serialize(bool includePersistent)
{
	const std::list<Entity*>& entities = EntityManager::get()->getEntities();

	WriteContext context;

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

	// Матеріали лежать у власних файлах, а рендер-компоненти посилаються на них шляхом
	JsonValue scene = JsonValue::object();

	scene.set("version", SCENE_VERSION);
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
	data.set("entities", entityList);

	return data;
}

// Записує вказані об'єкти з нащадками так само, як сцену: корені у світових координатах і зі зв'язками з префабами
std::string SceneSerializer::serializeEntities(const std::vector<Entity*>& roots)
{
	std::vector<Entity*> entities;

	for (Entity* root : roots)
	{
		collectSubtree(root, entities);
	}

	WriteContext context;

	for (size_t i = 0; i < entities.size(); i++)
	{
		context.indices[entities[i]] = (int)i;
	}

	JsonValue entityList = JsonValue::array();

	for (Entity* entity : entities)
	{
		entityList.push(writeEntity(entity, context, false, true));
	}

	JsonValue data = JsonValue::object();

	data.set("version", SCENE_VERSION);
	data.set("entities", entityList);

	return data.toString();
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

// Ставить рендер-компоненту матеріали слотів з опису: файли матеріалів за шляхом, а матеріали без файлу - з їхніх налаштувань
void SceneSerializer::applyMaterials(Renderer* renderer, const JsonValue& data, bool asTemplate, MaterialRegistry* registry)
{
	if (!data.has("materials")) return;

	const JsonValue& list = data.get("materials");

	// Слоти, яких немає в описі, малюються матеріалом за замовчуванням, як і в щойно доданого компонента
	renderer->clearMaterials();

	for (size_t i = 0; i < list.size(); i++)
	{
		const JsonValue& reference = list.at(i);

		Material* material = nullptr;

		if (reference.getType() == JsonValue::Type::String)
		{
			material = MaterialLibrary::get()->find(reference.asString());
		}
		else if (registry && reference.has("embedded"))
		{
			int id = reference.get("embedded").asInt(-1);

			if (id >= 0 && id < (int)registry->materials.size()) material = registry->materials[(size_t)id];
		}
		else if (reference.getType() == JsonValue::Type::Object)
		{
			// Образ префаба живе між сценами, тож і його власний матеріал не має зникати при їх зміні
			material = new Material();
			readMaterial(material, reference);
			material->dontDeleteOnLoad = asTemplate;
		}

		if (material) renderer->setMaterial((unsigned int)i, material);
	}
}

// Створює об'єкти з опису; кореневі стають дочірніми для parent
static std::vector<Entity*> buildEntities(const JsonValue& data, Entity* parent, bool asTemplate)
{
	const JsonValue& parsed = data.get("entities");

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
				SceneSerializer::applyMaterials(renderer, componentData, asTemplate);
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

	// Старі сцени тримали матеріали в собі; тепер вони переходять у файли матеріалів
	MaterialLibrary::get()->upgrade(scene);

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
	// одинаки на кшталт SceneChanger прибирають свою копію самі
	file << serialize(true);

	return true;
}
