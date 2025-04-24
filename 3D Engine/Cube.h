#pragma once
#include "RenderObject.h"
#include "Vector3.h"

class Cube : public RenderObject
{
public:
	Cube();
	Cube(Vector3 position);

	~Cube();

	virtual void render() override;

private:
	virtual void init() override;
};

