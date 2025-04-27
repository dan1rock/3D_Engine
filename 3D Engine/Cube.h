#pragma once
#include "RenderObject.h"

class Cube : public RenderObject
{
public:
	Cube();
	Cube(Vector3 position);

	~Cube();

	virtual void render() override;

private:
	virtual void init() override;

	VertexBuffer* mVertexBuffer = nullptr;
	IndexBuffer* mIndexBuffer = nullptr;
};

