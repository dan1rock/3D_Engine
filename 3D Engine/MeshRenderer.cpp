#include "MeshRenderer.h"

MeshRenderer::MeshRenderer()
{
}

MeshRenderer::MeshRenderer(Mesh* mesh)
{
	this->mMesh = mesh;
}

MeshRenderer::MeshRenderer(Mesh* mesh, Material* material)
{
	this->mMesh = mesh;
	this->mMaterial = material;
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

	if (mMesh == nullptr) return;

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
}
