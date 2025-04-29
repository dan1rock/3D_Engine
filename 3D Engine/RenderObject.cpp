#include "RenderObject.h"
#include "Material.h"

Matrix* RenderObject::getModelMatrix()
{
	return &modelM;
}

void RenderObject::init()
{
	this->modelM.setIdentity();

	modelM.setTranslation(this->position);

	if (mMaterial == nullptr) mMaterial = new Material();

	this->isInitialized = true;
}

void RenderObject::render()
{
	if (!isInitialized)
	{
		this->init();
	}

	GraphicsEngine::get()->setMaterial(mMaterial);
}

void RenderObject::setMaterial(Material* material)
{
	this->mMaterial = material;
}

void RenderObject::setMesh(Mesh* mesh)
{
	this->mMesh = mesh;
}
