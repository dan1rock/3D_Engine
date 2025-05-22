#include "GlobalResources.h"
#include "GraphicsEngine.h"
#include "Material.h"
#include "ConstantBuffer.h"

GlobalResources::GlobalResources()
{
}

// Ініціалізує глобальні ресурси: створює константний буфер і матеріал за замовчуванням
void GlobalResources::init()
{
    mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();
    mConstantBuffer->load(&mConstantData, sizeof(mConstantData));

    mDefaultMaterial = new Material();
	mDefaultMaterial->dontDeleteOnLoad = true;
}

// Звільняє глобальний константний буфер
GlobalResources::~GlobalResources()
{
    mConstantBuffer->release();
}

// Оновлює дані в константному буфері
void GlobalResources::updateConstantBuffer()
{
    mConstantBuffer->update(GraphicsEngine::get()->getImmDeviceContext(), &mConstantData);
}

// Повертає вказівник на структуру з глобальними константними даними
constant* GlobalResources::getConstantData()
{
    return &mConstantData;
}

// Повертає вказівник на глобальний константний буфер
ConstantBuffer* GlobalResources::getConstantBuffer()
{
    return mConstantBuffer;
}

// Повертає вказівник на матеріал за замовчуванням
Material* GlobalResources::getDefaultMaterial()
{
    return mDefaultMaterial;
}
