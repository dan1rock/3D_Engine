#include "SceneChanger.h"
#include "Entity.h"
#include "MainScene.h"
#include "Scene1.h"
#include "ScenePerformanceTest.h"
#include "SceneManager.h"
#include "Input.h"
#include "imgui.h"

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
	drawUI();

	if (Input::getKeyDown(VK_ESCAPE))
	{
		Input::hideCursor(!Input::isCursorHidden());
	}
}

void SceneChanger::drawUI()
{
	ImGui::Begin("Scene Changer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	if (ImGui::Button("Car Test", ImVec2(150, 0)))
	{
		SceneManager::get()->loadScene(new MainScene());
	}
	if (ImGui::Button("Collision Test", ImVec2(150, 0)))
	{
		SceneManager::get()->loadScene(new Scene1());
	}
	if (ImGui::Button("Performance Test", ImVec2(150, 0)))
	{
		SceneManager::get()->loadScene(new ScenePerformanceTest());
	}
	if (ImGui::Button("Exit", ImVec2(150, 0)))
	{
		SceneManager::get()->exit();
	}
	ImGui::End();
}
