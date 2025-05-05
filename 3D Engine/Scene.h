#pragma once

class Scene
{
public:
	Scene();
	virtual ~Scene();
	
	virtual void init() = 0;
};

