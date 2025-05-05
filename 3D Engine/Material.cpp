#include "Material.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "ConstantBuffer.h"
#include <iostream>

__declspec(align(16))
struct material {
	float ambient;
	float diffuse;
	float specular;
	float shininess;
	float color[4];
	bool isTextured;
};

material materialData = {};

Material::Material()
{
	mVertexShader = GraphicsEngine::get()->getVertexShader(L"VertexShader.hlsl", "main");
	mPixelShader = GraphicsEngine::get()->getPixelShader(L"PixelShader.hlsl", "main");

	mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();
	mConstantBuffer->load(&materialData, sizeof(materialData));

	mConstantBuffers[0] = GraphicsEngine::get()->getGlobalResources()->getConstantBuffer();
}

Material::~Material()
{
	mConstantBuffer->release();
}

void Material::setVertexShader(VertexShader* vertexShader)
{
	mVertexShader = vertexShader;
}

void Material::setPixelShader(PixelShader* pixelShader)
{
	mPixelShader = pixelShader;
}

void Material::addTexture(Texture* texture)
{
	if (texture == nullptr) return;
	mTextures.push_back(texture);
}

void Material::removeTexture(unsigned int id)
{
	if (id >= mTextures.size()) return;

	mTextures.erase(mTextures.begin() + id);
}

void Material::setConstantBuffer(ConstantBuffer* constantBuffer, int slot)
{
	if (slot == 1) return;
	mConstantBuffers[slot] = constantBuffer;
}

void Material::onMaterialSet()
{
	materialData.ambient = ambient;
	materialData.diffuse = 1.0f - smoothness;
	materialData.specular = smoothness;
	materialData.shininess = shininess;

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
