#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include "PixelShader.h"
#include "VertexShader.h"
#include "Matrix.h"

class ConstantBuffer;

class RenderObject
{
public:
	void setConstantBuffer(ConstantBuffer* constantBuffer);
	Matrix* getModelMatrix();
	virtual void render() = 0;
	void setTexture(Texture* texture);
	void setMesh(Mesh* mesh);

protected:
	virtual void init() = 0;

	Vector3 position;
	Matrix modelM;

	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;

	Mesh* mMesh = nullptr;

	Texture* mTexture = nullptr;

	bool isInitialized = false;
};

