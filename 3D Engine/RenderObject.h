#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include "Component.h"

class ConstantBuffer;
class Material;
class Matrix;

class RenderObject : public Component
{
public:
	RenderObject();
	~RenderObject() override;

	virtual void render() = 0;
	void setMaterial(Material* material);
	void setMesh(Mesh* mesh);

protected:
	virtual void init() = 0;

	Material* mMaterial = nullptr;

	Mesh* mMesh = nullptr;

	bool isInitialized = false;
};

