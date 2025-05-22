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

// Встановлює сцену для завантаження
void SceneManager::loadScene(Scene* scene)
{
	if (mRequestedScene != nullptr)
		return;

	mRequestedScene = scene;
}

// Повертає єдиний екземпляр менеджера сцен (синглтон)
SceneManager* SceneManager::get()
{
	static SceneManager instance;
	return &instance;
}

// Оновлює менеджер сцен, викликаючи завантаження нової сцени
void SceneManager::update()
{
	if (mRequestedScene == nullptr)
		return;

	std::wcout << L"Loading scene!" << std::endl;

	EntityManager::get()->onSceneLoadStart();
	mRequestedScene->init();
	EntityManager::get()->onSceneLoadFinished();

	std::wcout << L"Loaded scene!" << std::endl;

	delete mRequestedScene;
	mRequestedScene = nullptr;
}
