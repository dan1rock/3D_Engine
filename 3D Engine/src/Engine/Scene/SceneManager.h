#pragma once

class Scene;

class SceneManager
{
public:
	SceneManager();
	~SceneManager();

	// Повертає єдиний екземпляр менеджера сцен (синглтон)
	static SceneManager* get();

	// Встановлює сцену для завантаження
	void loadScene(Scene* scene);

private:
	Scene* mRequestedScene = nullptr;

	// Оновлює менеджер сцен, викликаючи завантаження нової сцени
	void update();

	friend class AppWindow;
};

