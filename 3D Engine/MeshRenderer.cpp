#include "MeshRenderer.h"

MeshRenderer::MeshRenderer()
{
	this->position = Vector3(0.0f, 0.0f, 0.0f);
}

MeshRenderer::MeshRenderer(Vector3 position)
{
	this->position = position;
}

MeshRenderer::MeshRenderer(Mesh* mesh)
{
	this->mMesh = mesh;
	this->position = Vector3(0.0f, 0.0f, 0.0f);
}

MeshRenderer::MeshRenderer(Mesh* mesh, Vector3 position)
{
	this->mMesh = mesh;
	this->position = position;
}

MeshRenderer::~MeshRenderer()
{
}

void MeshRenderer::init()
{
	RenderObject::init();
}

void MeshRenderer::render()
{
	RenderObject::render();

	GraphicsEngine::get()->getImmDeviceContext()->setVertexShader(mVertexShader);
	GraphicsEngine::get()->getImmDeviceContext()->setPixelShader(mPixelShader);

	GraphicsEngine::get()->getImmDeviceContext()->setTexture(mPixelShader, mTexture);

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
}
