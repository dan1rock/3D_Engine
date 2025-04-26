#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "Matrix.h"

class ConstantBuffer;

class RenderObject
{
public:
	void setConstantBuffer(ConstantBuffer* constantBuffer);
	Matrix* getModelMatrix();
	virtual void render() = 0;
	void setTexture(Texture* texture);

protected:
	virtual void init() = 0;

	Vector3 position;
	Matrix modelM;

	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;

	VertexBuffer* mVertexBuffer = nullptr;
	IndexBuffer* mIndexBuffer = nullptr;

	Texture* mTexture = nullptr;

	bool isInitialized = false;
};

