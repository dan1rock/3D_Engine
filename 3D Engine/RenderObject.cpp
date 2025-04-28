#include "RenderObject.h"

void RenderObject::setConstantBuffer(ConstantBuffer* constantBuffer)
{
	if (!isInitialized)
	{
		this->init();
	}

	GraphicsEngine::get()->getImmDeviceContext()->setConstantBuffer(mVertexShader, constantBuffer);
	GraphicsEngine::get()->getImmDeviceContext()->setConstantBuffer(mPixelShader, constantBuffer);
}

Matrix* RenderObject::getModelMatrix()
{
	return &modelM;
}

void RenderObject::init()
{
	this->modelM.setIdentity();

	if (mVertexShader == nullptr) mVertexShader = GraphicsEngine::get()->getVertexShader(L"VertexShader.hlsl", "main");
	if (mPixelShader == nullptr) mPixelShader = GraphicsEngine::get()->getPixelShader(L"PixelShader.hlsl", "main");

	modelM.setTranslation(this->position);

	this->isInitialized = true;
}

void RenderObject::render()
{
	if (!isInitialized)
	{
		this->init();
	}
}

void RenderObject::setTexture(Texture* texture)
{
	this->mTexture = texture;
}

void RenderObject::setMesh(Mesh* mesh)
{
	this->mMesh = mesh;
}
