#include "SceneManager.h"
#include "EntityManager.h"
#include "SceneSerializer.h"
#include "Entity.h"
#include "SkySphere.h"
#include "DirectionalLight.h"
#include "Camera.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace filesystem = std::experimental::filesystem;

const char* SceneManager::FOLDER = "Assets\\Scenes";
const char* SceneManager::EXTENSION = ".scene";

// Сцена без жодного об'єкта; з неї починається нова сцена
static const char* EMPTY_SCENE = "{ \"version\": 4, \"materials\": [], \"entities\": [] }";

SceneManager::SceneManager()
{
}

SceneManager::~SceneManager()
{
}

// Повертає єдиний екземпляр менеджера сцен (синглтон)
SceneManager* SceneManager::get()
{
	static SceneManager instance;
	return &instance;
}

// Перечитує перелік файлів сцен у теці
void SceneManager::refresh()
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

	// Порядок за ім'ям не залежить від файлової системи, тож номери сцен лишаються сталими
	std::sort(mPaths.begin(), mPaths.end());
}

// Повертає шляхи всіх сцен проєкту, упорядковані за ім'ям
const std::vector<std::string>& SceneManager::getScenePaths() const
{
	return mPaths;
}

// Повертає ім'я сцени без теки й розширення
std::string SceneManager::getSceneName(const std::string& path)
{
	return filesystem::path(path).stem().string();
}

// Шукає сцену за ім'ям або шляхом; повертає її шлях або порожній рядок, якщо такої немає
std::string SceneManager::findScene(const std::string& nameOrPath)
{
	refresh();

	for (const std::string& path : mPaths)
	{
		if (path == nameOrPath || getSceneName(path) == nameOrPath) return path;
	}

	// Сцена може лежати й поза стандартною текою, якщо до неї передали повний шлях
	std::error_code error;

	if (filesystem::is_regular_file(nameOrPath, error)) return nameOrPath;

	return std::string();
}

// Запитує завантаження сцени за ім'ям або шляхом
bool SceneManager::loadScene(const std::string& nameOrPath)
{
	std::string path = findScene(nameOrPath);

	if (path.empty())
	{
		std::cout << "Scene not found: " << nameOrPath << std::endl;
		return false;
	}

	// Кілька запитів за кадр: перемагає останній, як і в Unity
	mRequestedPath = path;

	return true;
}

// Запитує завантаження сцени за її номером у переліку getScenePaths
bool SceneManager::loadScene(int index)
{
	refresh();

	if (index < 0 || index >= (int)mPaths.size())
	{
		std::cout << "Scene index out of range: " << index << std::endl;
		return false;
	}

	return loadScene(mPaths[index]);
}

// Одразу замінює сцену вмістом файлу, прибравши й об'єкти з dontDestroyOnLoad
bool SceneManager::openScene(const std::string& path)
{
	// Запит, зроблений до відкриття, стосувався попередньої сцени
	mRequestedPath.clear();

	return loadFile(path, true);
}

// Створює файл нової сцени зі стандартним вмістом та одразу відкриває її
std::string SceneManager::createScene(const std::string& name)
{
	std::error_code error;
	filesystem::create_directories(FOLDER, error);

	std::string baseName = name.empty() ? "New Scene" : name;
	std::string path = std::string(FOLDER) + "\\" + baseName + EXTENSION;

	for (int number = 1; filesystem::exists(path, error); number++)
	{
		path = std::string(FOLDER) + "\\" + baseName + " " + std::to_string(number) + EXTENSION;
	}

	mRequestedPath.clear();

	// Порожня сцена прибирає все, що було, разом з об'єктами, які переживають зміну сцени
	SceneSerializer::deserialize(EMPTY_SCENE, nullptr, true);

	// Стандартний вміст, як у новій сцені Unity: небо, напрямлене світло та камера
	Entity* sky = new Entity();
	sky->setName("Sky");
	sky->addComponent<SkySphere>();

	Entity* light = new Entity();
	light->setName("Directional Light");
	light->getTransform()->setForward(Vector3(-0.55f, -1.0f, -0.35f));
	light->addComponent<DirectionalLight>();

	Entity* camera = new Entity(Vector3(0.0f, 1.0f, -10.0f));
	camera->setName("Main Camera");
	camera->addComponent<Camera>();

	mActivePath = path;
	mLoadCount++;

	if (!saveScene())
	{
		std::cout << "Failed to write scene " << path << std::endl;
		return std::string();
	}

	refresh();

	std::cout << "Created scene " << path << std::endl;

	return path;
}

// Зберігає поточну сцену у файл відкритої сцени
bool SceneManager::saveScene()
{
	if (mActivePath.empty()) return false;

	std::error_code error;
	filesystem::create_directories(FOLDER, error);

	return SceneSerializer::saveToFile(mActivePath);
}

// Відкриває першу сцену проєкту, а якщо сцен ще немає - створює сцену за замовчуванням
void SceneManager::loadStartupScene()
{
	refresh();

	if (mPaths.empty())
	{
		createScene("SampleScene");
		return;
	}

	loadScene(mPaths.front());
}

// Повертає шлях відкритої сцени
const std::string& SceneManager::getActiveScenePath() const
{
	return mActivePath;
}

// Повертає ім'я відкритої сцени
std::string SceneManager::getActiveSceneName() const
{
	return getSceneName(mActivePath);
}

// Повертає номер відкритої сцени у переліку getScenePaths, або -1
int SceneManager::getActiveSceneIndex() const
{
	for (size_t i = 0; i < mPaths.size(); i++)
	{
		if (mPaths[i] == mActivePath) return (int)i;
	}

	return -1;
}

// Робить вказаний файл відкритою сценою, нічого не завантажуючи
void SceneManager::setActiveScenePath(const std::string& path)
{
	mActivePath = path;
}

// Кількість завантажень сцени від запуску
unsigned int SceneManager::getLoadCount() const
{
	return mLoadCount;
}

void SceneManager::exit()
{
	mExitRequested = true;
}

// Читає файл сцени і замінює ним поточну
bool SceneManager::loadFile(const std::string& path, bool replacePersistent)
{
	std::ifstream file(path);

	if (!file.is_open())
	{
		std::cout << "Failed to open scene " << path << std::endl;
		return false;
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();

	std::cout << "Loading scene " << path << std::endl;

	// Зіпсований файл сцену не чіпає: серіалізатор спершу читає його повністю
	if (!SceneSerializer::deserialize(buffer.str(), nullptr, replacePersistent))
	{
		std::cout << "Failed to load scene " << path << std::endl;
		return false;
	}

	mActivePath = path;
	mLoadCount++;

	std::cout << "Loaded scene " << getSceneName(path) << std::endl;

	return true;
}

// Оновлює менеджер сцен, завантажуючи запитану сцену
void SceneManager::update()
{
	if (mRequestedPath.empty()) return;

	// Під час завантаження компоненти можуть запитати ще одну сцену, тож запит забирається заздалегідь
	std::string path = mRequestedPath;
	mRequestedPath.clear();

	loadFile(path, false);
}
