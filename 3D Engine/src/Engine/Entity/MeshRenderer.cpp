#include "MeshRenderer.h"

MeshRenderer::MeshRenderer()
{
}

// Конструктор класу MeshRenderer з вказаним мешем
MeshRenderer::MeshRenderer(Mesh* mesh)
{
	this->mMesh = mesh;
}

// Конструктор класу MeshRenderer з вказаними мешем та матеріалом
MeshRenderer::MeshRenderer(Mesh* mesh, Material* material)
{
	this->mMesh = mesh;
	this->mMaterial = material;
}

MeshRenderer::~MeshRenderer()
{
}

// Викликається при активації компонента
void MeshRenderer::awake()
{
	Renderer::awake();
}

// Викликається для рендеру об'єкта: встановлює буфери та викликає рендеринг
void MeshRenderer::render()
{
	Renderer::render();

	if (mMesh == nullptr) return;

	GraphicsEngine::get()->getImmDeviceContext()->setVertexBuffer(mMesh->getVertexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->setIndexBuffer(mMesh->getIndexBuffer());
	GraphicsEngine::get()->getImmDeviceContext()->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
}
