#pragma once

class Scene;

class SceneManager
{
public:
	SceneManager();
	~SceneManager();

	void loadScene(Scene* scene);

	static SceneManager* get();

private:
	Scene* mRequestedScene = nullptr;

	void update();

	friend class AppWindow;
};

