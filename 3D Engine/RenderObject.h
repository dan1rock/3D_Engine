#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include "Matrix.h"

class ConstantBuffer;
class Material;

class RenderObject
{
public:
	Matrix* getModelMatrix();
	virtual void render() = 0;
	void setMaterial(Material* material);
	void setMesh(Mesh* mesh);

protected:
	virtual void init() = 0;

	Vector3 position;
	Matrix modelM;

	Material* mMaterial = nullptr;

	Mesh* mMesh = nullptr;

	bool isInitialized = false;
};

