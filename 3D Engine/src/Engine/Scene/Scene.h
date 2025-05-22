#pragma once

// Базовий клас для всіх сцен в грі
class Scene
{
public:
	Scene();
	virtual ~Scene();
	
	// Ініціалізація сцени
	virtual void init() = 0;
};

