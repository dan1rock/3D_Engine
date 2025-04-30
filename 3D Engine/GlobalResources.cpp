#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "Material.h"
#include "ConstantBuffer.h"

GlobalResources::GlobalResources()
{
}

void GlobalResources::init()
{
    mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();
    mConstantBuffer->load(&mConstantData, sizeof(mConstantData));

    mDefaultMaterial = new Material();
}

GlobalResources::~GlobalResources()
{
    mConstantBuffer->release();
}

void GlobalResources::updateConstantBuffer()
{
    mConstantBuffer->update(GraphicsEngine::get()->getImmDeviceContext(), &mConstantData);
}

constant* GlobalResources::getConstantData()
{
    return &mConstantData;
}

ConstantBuffer* GlobalResources::getConstantBuffer()
{
    return mConstantBuffer;
}

Material* GlobalResources::getDefaultMaterial()
{
    return mDefaultMaterial;
}

