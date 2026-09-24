#include "SceneSerializer.h"
#include "SceneIO.h"
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

#include <sstream>
#include <iomanip>
#include <limits>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <iostream>

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

// Записує один матеріал рендер-компонента
static void writeMaterial(std::ostringstream& out, Material* material, int slot)
{
	if (material == nullptr) return;

	out << "material " << slot << "\n";
	out << "color " << material->color[0] << " " << material->color[1] << " "
		<< material->color[2] << " " << material->color[3] << "\n";
	out << "ambient " << material->ambient << "\n";
	out << "smoothness " << material->smoothness << "\n";
	out << "shininess " << material->shininess << "\n";
	out << "texturescale " << material->textureScale << "\n";
	out << "cullback " << (material->cullBack ? 1 : 0) << "\n";
	out << "clamptexture " << (material->clampTexture ? 1 : 0) << "\n";

	std::wstring texturePath = material->getTexturePath();

	if (!texturePath.empty()) out << "texture " << toRelativePath(texturePath) << "\n";

	// Нестандартний піксельний шейдер треба зберегти: інакше матеріал відновиться зі звичайним,
	// а той по-іншому змішує текстуру з кольором і малює, наприклад, прототипну сітку чорною
	std::wstring shaderPath = material->getPixelShaderPath();

	if (!shaderPath.empty()) out << "shader " << toRelativePath(shaderPath) << "\n";

	out << "endmaterial\n";
}

// Записує поточну сцену у текст
std::string SceneSerializer::serialize()
{
	std::ostringstream out;

	// Типові шість значущих цифр округлюють кути настільки, що після завантаження
	// об'єкт дивиться трохи в інший бік; max_digits10 дає точне повернення float
	out << std::setprecision(std::numeric_limits<float>::max_digits10);

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

	out << "scene 2\n";

	for (Entity* entity : entities)
	{
		if (entity->dontDestroyOnLoad) continue;

		out << "entity " << indices[entity] << "\n";
		out << "name " << entity->getName() << "\n";
		out << "active " << (entity->isActiveSelf ? 1 : 0) << "\n";

		// Образ треба відрізнити від звичайного об'єкта, бо інакше він оживе як окремий об'єкт сцени
		// і водночас компоненти, що на нього посилаються, лишаться без образу для створення копій
		if (dynamic_cast<Prefab*>(entity) != nullptr) out << "prefab 1\n";

		Entity* parent = entity->getParent();

		out << "parent " << (parent && indices.count(parent) ? indices[parent] : -1) << "\n";

		// Кореневий об'єкт не має відносно чого зберігати локальні координати, тому пишемо світові
		bool hasParent = parent != nullptr && indices.count(parent) > 0;

		Vector3 position = hasParent ? entity->getTransform()->getLocalPosition() : entity->getTransform()->getPosition();
		Vector3 rotation = hasParent ? entity->getTransform()->getLocalRotation() : entity->getTransform()->getRotation();
		Vector3 scale = hasParent ? entity->getTransform()->getLocalScale() : entity->getTransform()->getScale();

		out << "pos " << position.x << " " << position.y << " " << position.z << "\n";
		out << "rot " << rotation.x << " " << rotation.y << " " << rotation.z << "\n";
		out << "scl " << scale.x << " " << scale.y << " " << scale.z << "\n";

		for (Component* component : entity->getComponentList())
		{
			out << "component " << component->getTypeName() << "\n";

			// Рендер-компонент зберігає свій меш та матеріали кожного слота
			if (Renderer* renderer = dynamic_cast<Renderer*>(component))
			{
				if (renderer->getMesh())
				{
					out << "mesh " << toRelativePath(renderer->getMesh()->getFullPath()) << "\n";
				}

				out << "castshadows " << (renderer->castShadows ? 1 : 0) << "\n";

				unsigned int slots = renderer->getMaterialCount();

				for (unsigned int slot = 0; slot < slots; slot++)
				{
					writeMaterial(out, renderer->getMaterial(slot), (int)slot);
				}
			}

			// Решту полів компонент записує сам, бо лише він знає, що саме варто зберігати
			std::string fields;
			SceneWriter writer(fields, indices);
			component->serialize(writer);

			out << fields;

			out << "endcomponent\n";
		}

		out << "endentity\n";
	}

	return out.str();
}

// Матеріал, прочитаний з файлу, ще не перетворений на об'єкт рушія
struct MaterialData
{
	int slot = 0;
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float ambient = 0.4f;
	float smoothness = 0.5f;
	float shininess = 32.0f;
	float textureScale = 1.0f;
	bool cullBack = true;
	bool clampTexture = true;
	std::string texture;
	std::string shader;
};

// Компонент, прочитаний з файлу: ім'я типу та всі його поля у вигляді рядків
struct ComponentData
{
	std::string type;
	std::unordered_map<std::string, std::string> fields;
	std::vector<MaterialData> materials;
	bool hasMesh = false;
	std::string mesh;
	bool castShadows = true;
};

// Об'єкт, прочитаний з файлу, ще не створений у сцені
struct EntityData
{
	std::string name = "Entity";
	bool active = true;
	bool isPrefab = false;
	int parent = -1;
	Vector3 position = { 0.0f, 0.0f, 0.0f };
	Vector3 rotation = { 0.0f, 0.0f, 0.0f };
	Vector3 scale = { 1.0f, 1.0f, 1.0f };
	std::vector<ComponentData> components;
};

// Розбирає текст сцени у проміжні структури, нічого ще не створюючи у світі
static std::vector<EntityData> parseScene(const std::string& text)
{
	std::vector<EntityData> parsed;

	std::istringstream in(text);
	std::string line;

	EntityData* entity = nullptr;
	ComponentData* component = nullptr;
	MaterialData* material = nullptr;

	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r') line.pop_back();
		if (line.empty()) continue;

		std::istringstream tokens(line);
		std::string key;
		tokens >> key;

		// Решта рядка після ключа є значенням: воно може містити пробіли, як ім'я чи шлях
		std::string value;
		std::getline(tokens, value);

		if (!value.empty() && value.front() == ' ') value.erase(0, 1);

		if (key == "entity")
		{
			parsed.push_back(EntityData());
			entity = &parsed.back();
			component = nullptr;
			material = nullptr;
			continue;
		}

		if (entity == nullptr) continue;

		if (key == "component")
		{
			entity->components.push_back(ComponentData());
			component = &entity->components.back();
			component->type = value;
			material = nullptr;
			continue;
		}

		if (key == "endcomponent")
		{
			component = nullptr;
			material = nullptr;
			continue;
		}

		if (key == "material" && component)
		{
			component->materials.push_back(MaterialData());
			material = &component->materials.back();
			material->slot = atoi(value.c_str());
			continue;
		}

		if (key == "endmaterial")
		{
			material = nullptr;
			continue;
		}

		if (key == "endentity")
		{
			entity = nullptr;
			component = nullptr;
			material = nullptr;
			continue;
		}

		std::istringstream values(value);

		// Поля матеріалу читаються першими, бо вони перекривають однойменні ключі компонента
		if (material)
		{
			if (key == "color") values >> material->color[0] >> material->color[1] >> material->color[2] >> material->color[3];
			else if (key == "ambient") values >> material->ambient;
			else if (key == "smoothness") values >> material->smoothness;
			else if (key == "shininess") values >> material->shininess;
			else if (key == "texturescale") values >> material->textureScale;
			else if (key == "cullback") material->cullBack = atoi(value.c_str()) != 0;
			else if (key == "clamptexture") material->clampTexture = atoi(value.c_str()) != 0;
			else if (key == "texture") material->texture = value;
			else if (key == "shader") material->shader = value;

			continue;
		}

		if (component)
		{
			if (key == "mesh") { component->hasMesh = true; component->mesh = value; }
			else if (key == "castshadows") component->castShadows = atoi(value.c_str()) != 0;

			// Усі інші ключі зберігаються як є: їх розбере сам компонент у своєму deserialize
			component->fields[key] = value;

			continue;
		}

		if (key == "name") entity->name = value;
		else if (key == "active") entity->active = atoi(value.c_str()) != 0;
		else if (key == "prefab") entity->isPrefab = atoi(value.c_str()) != 0;
		else if (key == "parent") entity->parent = atoi(value.c_str());
		else if (key == "pos") values >> entity->position.x >> entity->position.y >> entity->position.z;
		else if (key == "rot") values >> entity->rotation.x >> entity->rotation.y >> entity->rotation.z;
		else if (key == "scl") values >> entity->scale.x >> entity->scale.y >> entity->scale.z;
	}

	return parsed;
}

// Створює матеріал одного слота рендер-компонента
static void buildMaterial(Renderer* renderer, const MaterialData& data)
{
	// Кожен слот отримує власний матеріал, щоб правки не розповзалися між об'єктами
	Material* material = new Material();

	material->color[0] = data.color[0];
	material->color[1] = data.color[1];
	material->color[2] = data.color[2];
	material->color[3] = data.color[3];
	material->ambient = data.ambient;
	material->smoothness = data.smoothness;
	material->shininess = data.shininess;
	material->textureScale = data.textureScale;
	material->cullBack = data.cullBack;
	material->clampTexture = data.clampTexture;

	if (!data.texture.empty())
	{
		material->addTexture(GraphicsEngine::get()->getTextureManager()->createTextureFromFile(toWide(data.texture).c_str()));
	}

	if (!data.shader.empty())
	{
		material->setPixelShader(GraphicsEngine::get()->getPixelShader(toWide(data.shader).c_str(), "main"));
	}

	renderer->setMaterial((unsigned int)data.slot, material);

	// Слот 0 стає і спільним матеріалом, щоб меші без поділу на частини теж малювалися ним
	if (data.slot == 0) renderer->setMaterial(material);
}

// Створює один компонент об'єкта за прочитаними даними
static Component* buildComponent(Entity* entity, const ComponentData& data, const std::vector<Entity*>& created)
{
	Component* component = nullptr;

	// Рухомість та маса фізичного тіла задаються лише конструктором, тому їх треба знати
	// ще до створення компонента, а не привласнювати потім у deserialize
	if (data.type == "RigidBody")
	{
		auto isStaticField = data.fields.find("static");
		auto massField = data.fields.find("mass");

		bool isStatic = isStaticField == data.fields.end() || atoi(isStaticField->second.c_str()) != 0;
		float mass = massField == data.fields.end() ? 1.0f : (float)atof(massField->second.c_str());

		component = isStatic ? entity->addComponent<RigidBody>(true) : entity->addComponent<RigidBody>(mass, false);
	}
	else
	{
		component = ComponentRegistry::create(data.type, entity);
	}

	if (component == nullptr)
	{
		std::cout << "Unknown component type in scene: " << data.type << std::endl;
		return nullptr;
	}

	if (Renderer* renderer = dynamic_cast<Renderer*>(component))
	{
		if (data.hasMesh)
		{
			renderer->setMesh(GraphicsEngine::get()->getMeshManager()->createMeshFromFile(toWide(data.mesh).c_str()));
		}

		renderer->castShadows = data.castShadows;

		for (const MaterialData& materialData : data.materials)
		{
			buildMaterial(renderer, materialData);
		}
	}

	SceneReader reader(data.fields, created);
	component->deserialize(reader);

	return component;
}

// Відновлює сцену з тексту, знищивши те, що було у сцені до цього
void SceneSerializer::deserialize(const std::string& text)
{
	std::vector<EntityData> parsed = parseScene(text);

	EntityManager::get()->onSceneLoadStart();

	// Спершу створюються всі об'єкти, щоб посилання компонентів було на що розв'язувати
	std::vector<Entity*> created;

	for (const EntityData& data : parsed)
	{
		Entity* entity = data.isPrefab ? new Prefab() : new Entity();

		entity->setName(data.name);
		entity->isActiveSelf = data.active;

		created.push_back(entity);
	}

	// Зв'язки батько-дитина встановлюються після створення всіх об'єктів
	for (size_t i = 0; i < created.size(); i++)
	{
		if (parsed[i].parent >= 0 && parsed[i].parent < (int)created.size())
		{
			created[i]->setParent(created[parsed[i].parent]);
		}
	}

	// Тепер відомо, чи має об'єкт батька, тому трансформацію можна застосувати правильно.
	// Зробити це треба до створення компонентів: коллайдер і фізичне тіло будуються за
	// поточним масштабом, тож із одиничним масштабом фізика вийшла б зовсім іншого розміру
	for (size_t i = 0; i < created.size(); i++)
	{
		Transform* transform = created[i]->getTransform();

		if (created[i]->getParent())
		{
			transform->setLocalScale(parsed[i].scale);
			transform->setLocalRotation(parsed[i].rotation);
			transform->setLocalPosition(parsed[i].position);
		}
		else
		{
			transform->setScale(parsed[i].scale);
			transform->setRotation(parsed[i].rotation);
			transform->setPosition(parsed[i].position);
		}
	}

	for (size_t i = 0; i < created.size(); i++)
	{
		for (const ComponentData& data : parsed[i].components)
		{
			buildComponent(created[i], data, created);
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
