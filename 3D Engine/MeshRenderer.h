#pragma once
#include "RenderObject.h"

class MeshRenderer : public RenderObject
{
public:
	MeshRenderer();
	MeshRenderer(Vector3 position);

	MeshRenderer(Mesh* mesh);
	MeshRenderer(Mesh* mesh, Vector3 position);

	~MeshRenderer();

	virtual void render() override;

private:
	virtual void init() override;
};
