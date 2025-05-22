#include "Material.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "ConstantBuffer.h"
#include "EntityManager.h"
#include <iostream>

__declspec(align(16))
struct material {
	float ambient;
	float diffuse;
	float specular;
	float shininess;
	float textureScale[4];
	float color[4];
	bool isTextured;
};

material materialData = {};

// Ініціалізує шейдери, константні буфери та реєструє матеріал
Material::Material()
{
	mVertexShader = GraphicsEngine::get()->getVertexShader(L"src\\Shaders\\VertexShader.hlsl", "main");
	mPixelShader = GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PixelShader.hlsl", "main");

	mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();
	mConstantBuffer->load(&materialData, sizeof(materialData));

	mConstantBuffers[0] = GraphicsEngine::get()->getGlobalResources()->getConstantBuffer();

	EntityManager::get()->registerMaterial(this);
}

// Звільняє ресурси та знімає реєстрацію матеріалу
Material::~Material()
{
	mConstantBuffer->release();

	EntityManager::get()->unregisterMaterial(this);
}

// Встановлює вершинний шейдер для матеріалу
void Material::setVertexShader(VertexShader* vertexShader)
{
	mVertexShader = vertexShader;
}

// Встановлює піксельний шейдер для матеріалу
void Material::setPixelShader(PixelShader* pixelShader)
{
	mPixelShader = pixelShader;
}

// Додає текстуру до матеріалу
void Material::addTexture(Texture* texture)
{
	if (texture == nullptr) return;
	mTextures.push_back(texture);
}

// Видаляє текстуру з матеріалу за індексом
void Material::removeTexture(unsigned int id)
{
	if (id >= mTextures.size()) return;

	mTextures.erase(mTextures.begin() + id);
}

// Встановлює колір матеріалу
void Material::setColor(float r, float g, float b, float a)
{
	color[0] = r;
	color[1] = g;
	color[2] = b;
	color[3] = a;
}

// Встановлює константний буфер для матеріалу у відповідний слот
void Material::setConstantBuffer(ConstantBuffer* constantBuffer, int slot)
{
	if (slot == 1) return;
	mConstantBuffers[slot] = constantBuffer;
}

// Оновлює дані матеріалу та встановлює константні буфери для шейдерів
void Material::onMaterialSet()
{
	materialData.ambient = ambient;
	materialData.diffuse = 1.0f - smoothness;
	materialData.specular = smoothness;
	materialData.shininess = shininess;
	materialData.textureScale[0] = textureScale;

	materialData.color[0] = color[0];
	materialData.color[1] = color[1];
	materialData.color[2] = color[2];
	materialData.color[3] = color[3];

	materialData.isTextured = mTextures.size() > 0;

	mConstantBuffer->update(GraphicsEngine::get()->getImmDeviceContext(), &materialData);
	mConstantBuffers[1] = mConstantBuffer;

	int count = sizeof(mConstantBuffers) / sizeof(mConstantBuffers[0]);

	for (int i = 0; i < count; i++)
	{
		if (mConstantBuffers[i] == nullptr) continue;

		GraphicsEngine::get()->getImmDeviceContext()->setConstantBuffer(mVertexShader, mConstantBuffers[i], i);
		GraphicsEngine::get()->getImmDeviceContext()->setConstantBuffer(mPixelShader, mConstantBuffers[i], i);
	}
}
