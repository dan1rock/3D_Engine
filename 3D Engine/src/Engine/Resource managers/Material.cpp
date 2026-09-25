#include "Material.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "ConstantBuffer.h"
#include "EntityManager.h"
#include "Texture.h"
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

// Копіює налаштування матеріалу, створюючи копії власний константний буфер та реєстрацію
Material::Material(const Material& material)
{
	mVertexShader = material.mVertexShader;
	mPixelShader = material.mPixelShader;

	// Текстури належать менеджеру текстур, тому копія користується тими самими
	mTextures = material.mTextures;

	copySettings(material);

	// Копія завжди належить сцені: інакше скопійований позначений матеріал ніколи б не звільнився
	dontDeleteOnLoad = false;

	// Власний буфер, бо в ньому зберігаються параметри саме цієї копії
	mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();
	mConstantBuffer->load(&materialData, sizeof(materialData));

	int count = sizeof(mConstantBuffers) / sizeof(mConstantBuffers[0]);

	for (int i = 0; i < count; i++)
	{
		mConstantBuffers[i] = material.mConstantBuffers[i];
	}

	// Нульовий слот завжди глобальний, а перший - власний буфер матеріалу
	mConstantBuffers[0] = GraphicsEngine::get()->getGlobalResources()->getConstantBuffer();
	mConstantBuffers[1] = mConstantBuffer;

	EntityManager::get()->registerMaterial(this);
}

// Копіює налаштування іншого матеріалу, зберігаючи власний константний буфер
Material& Material::operator=(const Material& material)
{
	if (this == &material) return *this;

	mVertexShader = material.mVertexShader;
	mPixelShader = material.mPixelShader;
	mTextures = material.mTextures;

	copySettings(material);

	// Нульовий та перший слоти лишаються власними, решту можна перенести
	int count = sizeof(mConstantBuffers) / sizeof(mConstantBuffers[0]);

	for (int i = 2; i < count; i++)
	{
		mConstantBuffers[i] = material.mConstantBuffers[i];
	}

	return *this;
}

// Переносить налаштування іншого матеріалу, не торкаючись власних ресурсів
void Material::copySettings(const Material& material)
{
	cullBack = material.cullBack;
	clampTexture = material.clampTexture;
	ambient = material.ambient;
	smoothness = material.smoothness;
	shininess = material.shininess;
	textureScale = material.textureScale;

	color[0] = material.color[0];
	color[1] = material.color[1];
	color[2] = material.color[2];
	color[3] = material.color[3];
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

// Повертає шлях до першої текстури матеріалу, або порожній рядок, якщо текстур немає
std::wstring Material::getTexturePath() const
{
	if (mTextures.empty() || mTextures[0] == nullptr) return std::wstring();

	return mTextures[0]->getFullPath();
}

// Повертає шлях до піксельного шейдера матеріалу, щоб сцена не втрачала нестандартний шейдер
std::wstring Material::getPixelShaderPath() const
{
	if (mPixelShader == nullptr) return std::wstring();

	return GraphicsEngine::get()->getPixelShaderPath(mPixelShader);
}

// Повертає кількість текстур матеріалу
unsigned int Material::getTextureCount() const
{
	return (unsigned int)mTextures.size();
}

// Повертає текстуру за номером
Texture* Material::getTexture(unsigned int index) const
{
	return index < mTextures.size() ? mTextures[index] : nullptr;
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
