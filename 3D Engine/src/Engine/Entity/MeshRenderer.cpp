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
	setMaterial(material);
}

MeshRenderer::~MeshRenderer()
{
}

// Викликається при активації компонента
void MeshRenderer::awake()
{
	Renderer::awake();
}

// Викликається для рендеру об'єкта: встановлює буфери та малює кожну частину меша її матеріалом
void MeshRenderer::render()
{
	Renderer::render();

	// Меш, який не вдалося завантажити, лишається без буферів, тому його пропускаємо
	if (mMesh == nullptr || mMesh->getIndexBuffer() == nullptr) return;

	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	deviceContext->setVertexBuffer(mMesh->getVertexBuffer());
	deviceContext->setIndexBuffer(mMesh->getIndexBuffer());

	const std::vector<SubMesh>& subMeshes = mMesh->getSubMeshes();

	// Меш без явного поділу малюється одним викликом, як і раніше
	if (subMeshes.empty())
	{
		applyMaterial(0);
		deviceContext->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
		return;
	}

	Material* lastMaterial = nullptr;

	for (const SubMesh& subMesh : subMeshes)
	{
		Material* material = resolveMaterial(subMesh.materialSlot);

		// Сусідні частини часто мають спільний матеріал, тому не перевстановлюємо його марно
		if (material != lastMaterial)
		{
			GraphicsEngine::get()->setMaterial(material);
			lastMaterial = material;
		}

		deviceContext->drawIndexedTriangleList(subMesh.indexCount, subMesh.indexStart, 0);
	}
}
