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

void SceneManager::loadScene(Scene* scene)
{
	if (mRequestedScene != nullptr)
		return;

	mRequestedScene = scene;
}

SceneManager* SceneManager::get()
{
	static SceneManager instance;
	return &instance;
}

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
