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

	// Завершує роботу менеджера сцен, виходячи з програми
	void exit();

private:
	Scene* mRequestedScene = nullptr;
	bool mExitRequested = false;

	// Оновлює менеджер сцен, викликаючи завантаження нової сцени
	void update();

	friend class AppWindow;
};

