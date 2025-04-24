#include "RenderObject.h"

void RenderObject::setConstantBuffer(ConstantBuffer* constantBuffer)
{
	if (!isInitialized)
	{
		this->init();
	}

	GraphicsEngine::engine()->getImmDeviceContext()->setConstantBuffer(mVertexShader, constantBuffer);
	GraphicsEngine::engine()->getImmDeviceContext()->setConstantBuffer(mPixelShader, constantBuffer);
}

Matrix* RenderObject::getModelMatrix()
{
	return &modelM;
}

void RenderObject::init()
{
	this->modelM.setIdentity();
	this->isInitialized = true;
}

void RenderObject::render()
{
	if (!isInitialized)
	{
		this->init();
	}
}
