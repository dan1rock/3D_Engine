#pragma once
#include "RenderObject.h"

class MeshRenderer : public RenderObject
{
public:
	MeshRenderer();
	MeshRenderer(Mesh* mesh);
	MeshRenderer(Mesh* mesh, Material* material);

	~MeshRenderer();

	virtual void render() override;

private:
	virtual void init() override;
};
