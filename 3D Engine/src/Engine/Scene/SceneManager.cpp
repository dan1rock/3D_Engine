#include "SceneManager.h"
#include "EntityManager.h"
#include "Scene.h"

#include <iostream>

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

// Встановлює сцену для завантаження
void SceneManager::loadScene(Scene* scene)
{
	if (mRequestedScene != nullptr)
		return;

	mRequestedScene = scene;
}

void SceneManager::exit()
{
	mExitRequested = true;
}

// Оновлює менеджер сцен, викликаючи завантаження нової сцени
void SceneManager::update()
{
	if (mRequestedScene == nullptr)
		return;

	std::wcout << L"Loading scene!" << std::endl;

	// Очищаємо менеджер сутностей перед завантаженням нової сцени
	EntityManager::get()->onSceneLoadStart();
	mRequestedScene->init();
	EntityManager::get()->onSceneLoadFinished();

	std::wcout << L"Loaded scene!" << std::endl;

	// Видаляємо запитану сцену
	delete mRequestedScene;
	mRequestedScene = nullptr;
}
