#include "RenderObject.h"
#include "Material.h"
#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "ComponentManager.h"

RenderObject::RenderObject()
{
	GraphicsEngine::get()->getComponentManager()->registerRenderer(this);
}

RenderObject::~RenderObject()
{
	GraphicsEngine::get()->getComponentManager()->unregisterRenderer(this);
}

Matrix* RenderObject::getModelMatrix()
{
	return &modelM;
}

void RenderObject::init()
{
	this->modelM.setIdentity();

	modelM.setTranslation(this->position);

	if (mMaterial == nullptr) mMaterial = GraphicsEngine::get()->getGlobalResources()->getDefaultMaterial();

	this->isInitialized = true;
}

void RenderObject::render()
{
	if (!isInitialized)
	{
		this->init();
	}

	GraphicsEngine::get()->setMaterial(mMaterial);
	
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();
	constantData->model = modelM;

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}

void RenderObject::setMaterial(Material* material)
{
	this->mMaterial = material;
}

void RenderObject::setMesh(Mesh* mesh)
{
	this->mMesh = mesh;
}
