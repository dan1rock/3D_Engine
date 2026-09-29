#include "MaterialLibrary.h"
#include "SceneSerializer.h"
#include "PrefabLibrary.h"
#include "SceneManager.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshManager.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "TextureManager.h"
#include "Texture.h"
#include "Renderer.h"
#include "EntityManager.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace filesystem = std::experimental::filesystem;

// Тека, у якій лежать файли матеріалів
const char* MaterialLibrary::FOLDER = "Assets\\Materials";
// Розширення файлів матеріалів
const char* MaterialLibrary::EXTENSION = ".mat";
// Назва вбудованого матеріалу за замовчуванням, якою на нього посилаються файли сцен
const char* MaterialLibrary::DEFAULT_NAME = "DefaultMaterial";

// Роздільник між шляхом моделі та іменем її матеріалу в посиланні, як-от Assets\Meshes\Car.fbx::Body
static const char* MODEL_SEPARATOR = "::";
// Розширення файлу налаштувань імпорту, що лежить поруч з моделлю
static const char* IMPORT_EXTENSION = ".import";
// Тека, у яку витягуються вбудовані текстури моделей
static const char* TEXTURE_FOLDER = "Assets\\Textures";

// Повертає шлях відносно теки проєкту, починаючи з Assets
static std::string relativePath(const std::wstring& fullPath)
{
	std::string path(fullPath.begin(), fullPath.end());
	size_t assets = path.rfind("Assets\\");

	return assets != std::string::npos ? path.substr(assets) : path;
}

// Читає весь файл у рядок; повертає false, якщо файл не відкрився
static bool readText(const std::string& path, std::string& text)
{
	std::ifstream file(path);

	if (!file.is_open()) return false;

	std::ostringstream buffer;
	buffer << file.rdbuf();
	text = buffer.str();

	return true;
}

// Записує рядок у файл; повертає false, якщо файл не відкрився
static bool writeText(const std::string& path, const std::string& text)
{
	std::ofstream file(path);

	if (!file.is_open()) return false;

	file << text;

	return true;
}

// Приводить шлях до зворотних скісних рисок, як їх пише рушій
static std::string normalizePath(std::string path)
{
	std::replace(path.begin(), path.end(), '/', '\\');

	return path;
}

// Повертає ім'я без символів, яких Windows не дозволяє в іменах файлів
static std::string sanitizeName(std::string name)
{
	for (char& symbol : name)
	{
		if (std::string("\\/:*?\"<>|").find(symbol) != std::string::npos) symbol = '_';
	}

	// Пробіли з країв Windows однаково відкидає, тож ім'я без них
	size_t first = name.find_first_not_of(' ');
	size_t last = name.find_last_not_of(' ');

	return first == std::string::npos ? std::string() : name.substr(first, last - first + 1);
}

// Повертає налаштування матеріалу текстом, за яким матеріали порівнюються між собою
static std::string settingsText(Material* material)
{
	return SceneSerializer::writeMaterial(material).toString();
}

// Повертає налаштування з опису матеріалу текстом у тому самому вигляді, що й у матеріалу з файлу
static std::string settingsText(const JsonValue& materialData)
{
	Material temporary;
	SceneSerializer::readMaterial(&temporary, materialData);

	return settingsText(&temporary);
}

// Повертає єдиний екземпляр бібліотеки (синглтон)
MaterialLibrary* MaterialLibrary::get()
{
	static MaterialLibrary instance;
	return &instance;
}

MaterialLibrary::MaterialLibrary()
{
	refresh();
}

// Перечитує перелік файлів матеріалів у теці
void MaterialLibrary::refresh()
{
	mPaths.clear();

	std::error_code error;

	if (!filesystem::exists(FOLDER, error)) return;

	for (const auto& entry : filesystem::directory_iterator(FOLDER, error))
	{
		if (!filesystem::is_regular_file(entry.path(), error)) continue;
		if (entry.path().extension().string() != EXTENSION) continue;

		mPaths.push_back(std::string(FOLDER) + "\\" + entry.path().filename().string());
	}

	std::sort(mPaths.begin(), mPaths.end());
}

// Повертає шляхи всіх знайдених матеріалів, упорядковані за ім'ям
const std::vector<std::string>& MaterialLibrary::getPaths() const
{
	return mPaths;
}

// Повертає ім'я матеріалу без теки й розширення
std::string MaterialLibrary::getName(const std::string& path)
{
	return filesystem::path(path).stem().string();
}

// Повертає матеріал з файлу, завантажуючи його лише раз; nullptr, якщо файлу немає чи він зіпсований
Material* MaterialLibrary::load(const std::string& requested)
{
	std::string path = normalizePath(requested);

	// Давнє посилання на перейменований матеріал веде до нового файлу
	auto renamed = mRenamed.find(path);
	if (renamed != mRenamed.end()) path = renamed->second;

	auto cached = mLoaded.find(path);

	if (cached != mLoaded.end()) return cached->second;

	std::string text;

	if (!readText(path, text)) return nullptr;

	std::string error;
	JsonValue data = JsonValue::parse(text, &error);

	if (!error.empty())
	{
		std::cout << "Material " << path << " is not valid: " << error << std::endl;
		return nullptr;
	}

	// Матеріал з файлу спільний для всіх сцен, тож зміна сцени його не видаляє
	Material* material = new Material();
	SceneSerializer::readMaterial(material, data);

	material->name = getName(path);
	material->assetPath = path;
	material->dontDeleteOnLoad = true;

	mLoaded[path] = material;
	mSaved[material] = settingsText(material);

	return material;
}

// Повертає матеріал за посиланням з файлу сцени; відсутній файл замінюється матеріалом за замовчуванням
Material* MaterialLibrary::find(const std::string& reference)
{
	Material* defaultMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	if (reference.empty() || reference == DEFAULT_NAME) return defaultMaterial;

	// Посилання модель::ім'я веде до матеріалу моделі або до файлу, яким його замінили
	size_t separator = reference.find(MODEL_SEPARATOR);

	if (separator != std::string::npos)
	{
		std::string modelPath = reference.substr(0, separator);
		std::string name = reference.substr(separator + 2);
		std::string remap = getRemap(modelPath, name);

		if (!remap.empty())
		{
			if (Material* material = load(remap)) return material;
		}

		if (Material* material = embeddedMaterial(modelPath, name)) return material;
	}
	else if (Material* material = load(reference))
	{
		return material;
	}

	std::cout << "Missing material " << reference << ", using " << DEFAULT_NAME << std::endl;

	return defaultMaterial;
}

// Повертає посилання на матеріал для файлу сцени: DefaultMaterial, шлях файлу, або порожнє для матеріалу без файлу
std::string MaterialLibrary::referenceOf(Material* material)
{
	if (isDefault(material)) return DEFAULT_NAME;

	return material ? material->assetPath : std::string();
}

// Повертає посилання так, як його записано в тексті файлу JSON: у лапках і з подвоєними скісними рисками
std::string MaterialLibrary::quotedReference(const std::string& reference)
{
	std::string text = JsonValue(reference).toString();

	// Запис значення закінчується переходом на новий рядок, якого всередині файлу після посилання немає
	while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();

	return text;
}

// Перевіряє, чи це вбудований матеріал за замовчуванням, який не редагується
bool MaterialLibrary::isDefault(Material* material)
{
	return material != nullptr && material == GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();
}

// Перевіряє, чи матеріал вбудований у файл моделі й тому лише для перегляду
bool MaterialLibrary::isEmbedded(Material* material)
{
	return material != nullptr && material->assetPath.find(MODEL_SEPARATOR) != std::string::npos;
}

// Повертає шлях моделі, у яку вбудовано матеріал, або порожній рядок
std::string MaterialLibrary::embeddedModel(Material* material)
{
	if (!isEmbedded(material)) return std::string();

	return material->assetPath.substr(0, material->assetPath.find(MODEL_SEPARATOR));
}

// Повертає шлях файлу моделі меша відносно теки проєкту, без імені вузла
std::string MaterialLibrary::modelPathOf(Mesh* mesh)
{
	if (mesh == nullptr) return std::string();

	std::string path = relativePath(mesh->getFullPath());

	return path.substr(0, path.find(MODEL_SEPARATOR));
}

// Повертає імена матеріалів моделі за слотами; порожнє ім'я означає слот, якого модель не описує
std::vector<std::string> MaterialLibrary::modelMaterialNames(Mesh* mesh)
{
	std::vector<std::string> names;

	if (mesh == nullptr) return names;

	const std::vector<ModelMaterial>& materials = mesh->getModelMaterials();

	for (unsigned int slot = 0; slot < (unsigned int)materials.size(); slot++)
	{
		if (!materials[slot].imported)
		{
			names.push_back(std::string());
			continue;
		}

		// Ключ матеріалу однаковий у всій моделі і в кожному її вузлі, тож посилання ведуть до того самого матеріалу
		std::string name = sanitizeName(materials[slot].key);

		names.push_back(name.empty() ? std::string("Material") : name);
	}

	return names;
}

// Повертає матеріал слота моделі: заміну з налаштувань імпорту або вбудований; nullptr, якщо модель слота не описує
Material* MaterialLibrary::modelMaterial(Mesh* mesh, unsigned int slot)
{
	std::vector<std::string> names = modelMaterialNames(mesh);

	if (slot >= names.size() || names[slot].empty()) return nullptr;

	return find(modelPathOf(mesh) + MODEL_SEPARATOR + names[slot]);
}

// Повертає вбудований матеріал моделі за іменем, не зважаючи на заміну
Material* MaterialLibrary::embeddedMaterial(const std::string& modelPath, const std::string& name)
{
	std::string reference = modelPath + MODEL_SEPARATOR + name;

	auto cached = mEmbedded.find(reference);

	if (cached != mEmbedded.end()) return cached->second;

	std::wstring wide(modelPath.begin(), modelPath.end());
	Mesh* mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(wide.c_str());

	std::vector<std::string> names = modelMaterialNames(mesh);
	auto found = std::find(names.begin(), names.end(), name);

	if (found == names.end()) return nullptr;

	const ModelMaterial& source = mesh->getModelMaterials()[found - names.begin()];

	// Матеріал моделі живе, поки живе проєкт, і не редагується, як файл моделі
	Material* material = new Material();
	material->properties = source.properties;
	material->name = name;
	material->assetPath = reference;
	material->dontDeleteOnLoad = true;

	for (int i = 0; i < (int)MaterialMap::Count; i++)
	{
		if (!source.maps[i].empty()) material->setMap((MaterialMap)i, GraphicsEngine::get()->getTextureManager()->createTextureFromFile(source.maps[i].c_str()));
	}

	mEmbedded[reference] = material;

	return material;
}

// Ставить рендер-компоненту матеріали моделі в усі слоти, як їх описує її файл
void MaterialLibrary::applyModelMaterials(Renderer* renderer)
{
	renderer->clearMaterials();

	for (unsigned int slot = 0; slot < renderer->getMaterialCount(); slot++)
	{
		if (Material* material = modelMaterial(renderer->getMesh(), slot)) renderer->setMaterial(slot, material);
	}
}

// Повертає заміни вбудованих матеріалів моделі, прочитавши її файл налаштувань імпорту за потреби
std::map<std::string, std::string>& MaterialLibrary::remapsOf(const std::string& modelPath)
{
	auto cached = mRemaps.find(modelPath);

	if (cached != mRemaps.end()) return cached->second;

	std::map<std::string, std::string>& remaps = mRemaps[modelPath];

	std::string text;

	if (readText(modelPath + IMPORT_EXTENSION, text))
	{
		const JsonValue& materials = JsonValue::parse(text).get("materials");

		for (size_t i = 0; i < materials.size(); i++)
		{
			remaps[materials.keyAt(i)] = materials.valueAt(i).asString();
		}
	}

	return remaps;
}

// Повертає файл матеріалу, яким замінено вбудований матеріал моделі, або порожній рядок
std::string MaterialLibrary::getRemap(const std::string& modelPath, const std::string& name)
{
	std::map<std::string, std::string>& remaps = remapsOf(modelPath);
	auto found = remaps.find(name);

	if (found == remaps.end()) return std::string();

	// Заміна, чий файл перейменували, веде до нового імені
	auto renamed = mRenamed.find(found->second);

	return renamed != mRenamed.end() ? renamed->second : found->second;
}

// Замінює вбудований матеріал моделі файлом матеріалу і оновлює об'єкти сцени; порожній шлях прибирає заміну
void MaterialLibrary::setRemap(const std::string& modelPath, const std::string& name, const std::string& materialPath)
{
	std::map<std::string, std::string>& remaps = remapsOf(modelPath);

	std::string previous = getRemap(modelPath, name);

	if (materialPath.empty()) remaps.erase(name);
	else remaps[name] = materialPath;

	// Налаштування імпорту лежать поруч з моделлю, як і в будь-якого ресурсу, що має власні налаштування
	JsonValue materials = JsonValue::object();

	for (const auto& entry : remaps)
	{
		materials.set(entry.first.c_str(), entry.second);
	}

	JsonValue data = JsonValue::object();
	data.set("version", 1);
	data.set("materials", materials);

	std::error_code error;

	if (remaps.empty()) filesystem::remove(modelPath + IMPORT_EXTENSION, error);
	else writeText(modelPath + IMPORT_EXTENSION, data.toString());

	// Об'єкти сцени, що малювались вбудованим чи попереднім матеріалом слота, переходять на новий
	Material* embedded = embeddedMaterial(modelPath, name);
	Material* old = previous.empty() ? embedded : load(previous);
	Material* wanted = find(modelPath + MODEL_SEPARATOR + name);

	for (Renderer* renderer : EntityManager::get()->getRenderers())
	{
		for (unsigned int slot = 0; slot < renderer->getMaterialCount(); slot++)
		{
			Material* current = renderer->getSharedMaterial(slot);

			if (current == embedded || (old && current == old && modelPathOf(renderer->getMesh()) == modelPath)) renderer->setMaterial(slot, wanted);
		}
	}
}

// Витягує вбудовані матеріали моделі у файли разом з вбудованими текстурами й замінює ними вбудовані; повертає кількість
int MaterialLibrary::extractMaterials(const std::string& modelPath)
{
	std::wstring wide(modelPath.begin(), modelPath.end());
	Mesh* mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(wide.c_str());

	if (mesh == nullptr) return 0;

	std::string modelName = sanitizeName(getName(modelPath));

	// Кожна вбудована текстура записується у файл лише раз, навіть якщо нею користуються кілька матеріалів
	std::map<Texture*, Texture*> extractedTextures;
	int count = 0;

	for (const std::string& name : modelMaterialNames(mesh))
	{
		if (name.empty() || !getRemap(modelPath, name).empty()) continue;

		Material* embedded = embeddedMaterial(modelPath, name);

		if (embedded == nullptr) continue;

		std::string path = createAsset(name, embedded);
		Material* asset = load(path);

		if (asset == nullptr) continue;

		// Текстури з файлу моделі стають звичайними файлами текстур, щоб матеріал не залежав від моделі
		for (int i = 0; i < (int)MaterialMap::Count; i++)
		{
			Texture* texture = asset->getMap((MaterialMap)i);

			if (texture == nullptr) continue;

			std::wstring texturePath = texture->getFullPath();
			size_t separator = texturePath.find(L"::");

			// Текстура з-поза проєкту копіюється в теку текстур, бо файл матеріалу посилається лише на файли проєкту
			if (separator == std::wstring::npos)
			{
				std::error_code error;
				std::wstring projectAssets = filesystem::absolute(L"Assets").wstring() + L"\\";

				if (texturePath.compare(0, projectAssets.size(), projectAssets) == 0) continue;

				if (extractedTextures.count(texture) == 0)
				{
					filesystem::create_directories(TEXTURE_FOLDER, error);

					std::wstring folder(TEXTURE_FOLDER, TEXTURE_FOLDER + strlen(TEXTURE_FOLDER));
					std::wstring copy = folder + L"\\" + filesystem::path(texturePath).filename().wstring();

					if (!filesystem::exists(copy, error)) filesystem::copy_file(texturePath, copy, error);

					extractedTextures[texture] = filesystem::exists(copy, error) ? GraphicsEngine::get()->getTextureManager()->createTextureFromFile(copy.c_str()) : nullptr;
				}

				if (extractedTextures[texture]) asset->setMap((MaterialMap)i, extractedTextures[texture]);

				continue;
			}

			if (extractedTextures.count(texture) == 0)
			{
				const EmbeddedTexture* source = mesh->findEmbeddedTexture(texturePath.substr(separator + 2));

				if (source == nullptr) continue;

				// Ім'я файлу береться з імені, під яким текстуру вбудували, або з імені моделі та номера
				std::string stem = source->name.empty() ? modelName + " " + std::string(texturePath.begin() + separator + 3, texturePath.end()) : sanitizeName(filesystem::path(source->name).stem().string());

				std::error_code error;
				filesystem::create_directories(TEXTURE_FOLDER, error);

				std::wstring extension = Texture::embeddedExtension(*source);
				std::wstring base = std::wstring(stem.begin(), stem.end());
				std::wstring folder(TEXTURE_FOLDER, TEXTURE_FOLDER + strlen(TEXTURE_FOLDER));
				std::wstring file = folder + L"\\" + base + extension;

				for (int number = 1; filesystem::exists(file, error); number++)
				{
					file = folder + L"\\" + base + L" " + std::to_wstring(number) + extension;
				}

				extractedTextures[texture] = Texture::writeEmbedded(*source, file) ? GraphicsEngine::get()->getTextureManager()->createTextureFromFile(file.c_str()) : nullptr;
			}

			if (extractedTextures[texture]) asset->setMap((MaterialMap)i, extractedTextures[texture]);
		}

		saveChanged();

		setRemap(modelPath, name, path);
		count++;
	}

	return count;
}

// Повертає всі вже завантажені матеріали з файлів
std::vector<Material*> MaterialLibrary::getLoaded() const
{
	std::vector<Material*> out;

	for (const auto& entry : mLoaded)
	{
		out.push_back(entry.second);
	}

	return out;
}

// Повертає вільний шлях файлу для імені, доповнюючи ім'я номером, якщо такий файл уже є
std::string MaterialLibrary::uniquePath(const std::string& name) const
{
	std::error_code error;

	std::string path = std::string(FOLDER) + "\\" + name + EXTENSION;

	for (int number = 1; filesystem::exists(path, error); number++)
	{
		path = std::string(FOLDER) + "\\" + name + " " + std::to_string(number) + EXTENSION;
	}

	return path;
}

// Записує матеріал у його файл
bool MaterialLibrary::write(Material* material)
{
	if (material->assetPath.empty()) return false;

	JsonValue settings = SceneSerializer::writeMaterial(material);

	// Номер формату стоїть першим, як у файлах сцен
	JsonValue out = JsonValue::object();
	out.set("version", 1);

	for (size_t i = 0; i < settings.size(); i++)
	{
		out.set(settings.keyAt(i).c_str(), settings.valueAt(i));
	}

	if (!writeText(material->assetPath, out.toString()))
	{
		std::cout << "Failed to write material " << material->assetPath << std::endl;
		return false;
	}

	mSaved[material] = settingsText(material);

	return true;
}

// Створює файл матеріалу з налаштуваннями source або типовими; повертає шлях або порожній рядок
std::string MaterialLibrary::createAsset(const std::string& name, Material* source)
{
	std::error_code error;
	filesystem::create_directories(FOLDER, error);

	std::string cleanName = sanitizeName(name);

	if (cleanName.empty()) cleanName = "New Material";

	std::string path = uniquePath(cleanName);

	// Новий матеріал спершу живе в пам'яті, а потім записується так само, як будь-який змінений
	Material* material = source ? new Material(*source) : new Material();

	material->name = getName(path);
	material->assetPath = path;
	material->dontDeleteOnLoad = true;

	if (!write(material))
	{
		delete material;
		return std::string();
	}

	mLoaded[path] = material;

	refresh();

	std::cout << "Created material " << path << std::endl;

	return path;
}

// Замінює в усіх файлах теки одне посилання іншим
static void replaceInFiles(const char* folder, const char* extension, const std::string& from, const std::string& to)
{
	std::error_code error;

	if (!filesystem::exists(folder, error)) return;

	for (const auto& entry : filesystem::directory_iterator(folder, error))
	{
		if (entry.path().extension().string() != extension) continue;

		std::string path = entry.path().string();
		std::string text;

		if (!readText(path, text) || text.find(from) == std::string::npos) continue;

		for (size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
		{
			text.replace(at, from.size(), to);
		}

		writeText(path, text);
	}
}

// Перейменовує файл матеріалу й виправляє посилання у файлах сцен і префабів; повертає новий шлях або порожній рядок
std::string MaterialLibrary::rename(Material* material, const std::string& newName)
{
	if (material == nullptr || material->assetPath.empty()) return std::string();

	std::string cleanName = sanitizeName(newName);

	if (cleanName.empty() || cleanName == material->name) return std::string();

	std::string oldPath = material->assetPath;
	std::string newPath = std::string(FOLDER) + "\\" + cleanName + EXTENSION;

	std::error_code error;

	if (filesystem::exists(newPath, error))
	{
		std::cout << "Material " << newPath << " already exists" << std::endl;
		return std::string();
	}

	filesystem::rename(oldPath, newPath, error);

	if (error)
	{
		std::cout << "Failed to rename material " << oldPath << std::endl;
		return std::string();
	}

	mLoaded.erase(oldPath);
	mLoaded[newPath] = material;

	material->assetPath = newPath;
	material->name = cleanName;

	// Давні посилання, зокрема на ще давніші імена, тепер ведуть до нового файлу
	for (auto& entry : mRenamed)
	{
		if (entry.second == oldPath) entry.second = newPath;
	}

	mRenamed[oldPath] = newPath;

	// Посилання у файлах записані рядками JSON, тож і шукаються в тому самому вигляді
	std::string from = quotedReference(oldPath);
	std::string to = quotedReference(newPath);

	replaceInFiles(SceneManager::FOLDER, SceneManager::EXTENSION, from, to);
	replaceInFiles(PrefabLibrary::FOLDER, ".prefab", from, to);

	// Префаби з пам'яті ще посилаються на старий шлях, тож перечитуються з виправлених файлів
	PrefabLibrary::get()->forgetData();

	refresh();

	return newPath;
}

// Записує у файли матеріали, змінені від останнього запису
void MaterialLibrary::saveChanged()
{
	for (const auto& entry : mLoaded)
	{
		Material* material = entry.second;

		if (mSaved[material] != settingsText(material)) write(material);
	}
}

// Повертає файл матеріалу з такими самими налаштуваннями, як в описі, або створює новий з вказаним ім'ям
std::string MaterialLibrary::adopt(const JsonValue& materialData, const std::string& name)
{
	std::string wanted = settingsText(materialData);

	// Однакові матеріали з різних сцен стають одним файлом, тож повторний перехід нових файлів не створює
	for (const std::string& path : mPaths)
	{
		Material* existing = load(path);

		if (existing && settingsText(existing) == wanted) return path;
	}

	Material source;
	SceneSerializer::readMaterial(&source, materialData);

	return createAsset(name, &source);
}

// Переводить старі сцени й префаби з вбудованими матеріалами на файли матеріалів; повертає, чи опис змінився
bool MaterialLibrary::upgrade(JsonValue& data)
{
	if (data.getType() != JsonValue::Type::Object || data.get("version").asInt(0) >= REFERENCE_VERSION) return false;

	const JsonValue table = data.get("materials");
	std::vector<std::string> tablePaths(table.size());

	bool changed = false;

	const JsonValue& entities = data.get("entities");
	JsonValue entityList = JsonValue::array();

	for (size_t e = 0; e < entities.size(); e++)
	{
		JsonValue entity = entities.at(e);
		std::string entityName = sanitizeName(entity.get("name").asString("Material"));

		if (entityName.empty()) entityName = "Material";

		const JsonValue& components = entity.get("components");
		JsonValue componentList = JsonValue::array();

		for (size_t c = 0; c < components.size(); c++)
		{
			JsonValue component = components.at(c);

			bool hasShared = component.has("material");
			const JsonValue& slotList = component.get("slotMaterials");
			const JsonValue& legacyList = component.get("materials");

			if (!hasShared && slotList.size() == 0 && legacyList.size() == 0)
			{
				componentList.push(component);
				continue;
			}

			// Кількість слотів береться з меша, бо старий формат не записував тих, що брали спільний матеріал
			Mesh* mesh = nullptr;
			std::string meshPath = component.get("mesh").asString();

			if (!meshPath.empty())
			{
				std::wstring wide(meshPath.begin(), meshPath.end());
				mesh = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(wide.c_str());
			}

			unsigned int slotCount = mesh && mesh->getMaterialCount() > 0 ? mesh->getMaterialCount() : 1;

			// Ім'я нового файлу: ім'я об'єкта, а для окремого слота ще й ім'я частини меша
			auto nameFor = [&](int slot) -> std::string {
				if (slot < 0 || slotCount < 2 || mesh == nullptr || mesh->getMaterialName((unsigned int)slot).empty()) return entityName;
				return entityName + " " + sanitizeName(mesh->getMaterialName((unsigned int)slot));
			};

			// Посилання на матеріал з таблиці; -1 означав матеріал за замовчуванням
			auto tableReference = [&](int index, int slot) -> std::string {
				if (index == -1) return DEFAULT_NAME;
				if (index < 0 || index >= (int)table.size()) return std::string();
				if (tablePaths[(size_t)index].empty()) tablePaths[(size_t)index] = adopt(table.at((size_t)index), nameFor(slot));
				return tablePaths[(size_t)index];
			};

			std::string shared = hasShared ? tableReference(component.get("material").asInt(-2), -1) : std::string();
			std::vector<std::string> slots(slotCount);

			for (size_t s = 0; s < slotList.size(); s++)
			{
				int slot = slotList.at(s).get("slot").asInt(0);

				if (slot < 0) continue;
				if ((size_t)slot >= slots.size()) slots.resize((size_t)slot + 1);

				slots[(size_t)slot] = tableReference(slotList.at(s).get("material").asInt(-2), slot);
			}

			// Файли третьої версії тримали матеріали прямо в компоненті, окремо для кожного слота
			for (size_t s = 0; s < legacyList.size(); s++)
			{
				int slot = legacyList.at(s).get("slot").asInt(0);

				if (slot < 0) continue;
				if ((size_t)slot >= slots.size()) slots.resize((size_t)slot + 1);

				slots[(size_t)slot] = adopt(legacyList.at(s), nameFor(slot));

				if (slot == 0 && shared.empty()) shared = slots[0];
			}

			// Слот без власного матеріалу брав спільний, а без спільного - матеріал за замовчуванням
			JsonValue references = JsonValue::array();

			for (const std::string& slot : slots)
			{
				references.push(!slot.empty() ? slot : !shared.empty() ? shared : std::string(DEFAULT_NAME));
			}

			component.remove("material");
			component.remove("slotMaterials");
			component.set("materials", references);

			componentList.push(component);
			changed = true;
		}

		if (components.size() > 0) entity.set("components", componentList);

		// Позначки змінених полів екземпляра переходять на нове поле матеріалів
		if (entity.has("prefabOverrides"))
		{
			const JsonValue& overrides = entity.get("prefabOverrides");
			std::vector<std::string> keys;

			for (size_t k = 0; k < overrides.size(); k++)
			{
				std::string key = overrides.at(k).asString();

				for (const char* old : { ".material", ".slotMaterials" })
				{
					size_t length = std::string(old).size();

					if (key.size() > length && key.compare(key.size() - length, length, old) == 0)
					{
						key = key.substr(0, key.size() - length) + ".materials";
						changed = true;
					}
				}

				if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
			}

			JsonValue overrideList = JsonValue::array();

			for (const std::string& key : keys)
			{
				overrideList.push(key);
			}

			entity.set("prefabOverrides", overrideList);
		}

		entityList.push(entity);
	}

	if (table.size() > 0) changed = true;

	data.set("entities", entityList);
	data.remove("materials");
	data.set("version", REFERENCE_VERSION);

	return changed;
}

// Переводить файл сцени чи префаба на файли матеріалів і перезаписує його, якщо було що переводити
bool MaterialLibrary::upgradeFile(const std::string& path)
{
	std::string text;

	if (!readText(path, text)) return false;

	std::string error;
	JsonValue data = JsonValue::parse(text, &error);

	if (!error.empty() || !upgrade(data)) return false;

	if (!writeText(path, data.toString())) return false;

	std::cout << "Moved materials of " << path << " to " << FOLDER << std::endl;

	return true;
}
