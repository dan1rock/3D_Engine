#include "SceneChanger.h"
#include "Entity.h"
#include "MainScene.h"
#include "Scene1.h"
#include "ScenePerformanceTest.h"
#include "SceneManager.h"
#include "Input.h"

SceneChanger* instance = nullptr;

SceneChanger::SceneChanger()
{
}

SceneChanger::~SceneChanger()
{
	if (instance == this)
	{
		instance = nullptr;
	}
}

void SceneChanger::awake()
{
	if (instance != nullptr)
	{
		mOwner->destroy();
		return;
	}

	instance = this;
	mOwner->dontDestroyOnLoad = true;

	Input::hideCursor(true);
}

void SceneChanger::update()
{
	if (Input::getKeyDown('1'))
	{
		SceneManager::get()->loadScene(new MainScene());
	}
	if (Input::getKeyDown('2'))
	{
		SceneManager::get()->loadScene(new Scene1());
	}
	if (Input::getKeyDown('3'))
	{
		SceneManager::get()->loadScene(new ScenePerformanceTest());
	}

	if (Input::getKeyDown(VK_ESCAPE))
	{
		Input::hideCursor(!Input::isCursorHidden());
	}
}
