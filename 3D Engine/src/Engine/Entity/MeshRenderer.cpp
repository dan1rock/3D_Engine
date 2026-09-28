#include "MeshRenderer.h"
#include "Material.h"

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

	// Кожна частина малюється лише у своєму проході: непрозорі першими, прозорі після них
	bool transparentPass = GraphicsEngine::get()->getRenderPass() == RenderPass::Transparent;

	// Меш без явного поділу малюється одним викликом, як і раніше
	if (subMeshes.empty())
	{
		if (resolveMaterial(0)->isTransparent() != transparentPass) return;

		applyMaterial(0);
		deviceContext->drawIndexedTriangleList(mMesh->getIndexBuffer()->getVertexListSize(), 0, 0);
		return;
	}

	Material* lastMaterial = nullptr;

	for (const SubMesh& subMesh : subMeshes)
	{
		Material* material = resolveMaterial(subMesh.materialSlot);

		if (material->isTransparent() != transparentPass) continue;

		// Сусідні частини часто мають спільний матеріал, тому не перевстановлюємо його марно
		if (material != lastMaterial)
		{
			GraphicsEngine::get()->setMaterial(material);
			lastMaterial = material;
		}

		deviceContext->drawIndexedTriangleList(subMesh.indexCount, subMesh.indexStart, 0);
	}
}
