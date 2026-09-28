#include "Material.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "ConstantBuffer.h"
#include "EntityManager.h"
#include "Texture.h"
#include <iostream>

// Дані матеріалу для шейдера; кожне поле займає цілий регістр із чотирьох чисел, як у cbuffer
__declspec(align(16))
struct material {
	float baseColor[4];
	float specularColor[4];
	float emissionColor[4];
	float tilingOffset[4];
	float detailTilingOffset[4];
	// Металевість, гладкість, сила нормалей і висота паралаксу
	float surface[4];
	// Сила затінення, поріг альфи, сила деталей кольору й нормалей
	float extra[4];
	// Наявні карти, увімкнені можливості, режим змішування
	unsigned int flags[4];
};

// Біти наявних карт; мають збігатися з Lit.hlsli
static const unsigned int MAP_BASE = 1;
static const unsigned int MAP_GLOSS = 2;
static const unsigned int MAP_NORMAL = 4;
static const unsigned int MAP_HEIGHT = 8;
static const unsigned int MAP_OCCLUSION = 16;
static const unsigned int MAP_EMISSION = 32;
static const unsigned int MAP_DETAIL_MASK = 64;
static const unsigned int MAP_DETAIL_ALBEDO = 128;
static const unsigned int MAP_DETAIL_NORMAL = 256;

// Біти можливостей матеріалу; мають збігатися з Lit.hlsli
static const unsigned int FEATURE_SPECULAR_WORKFLOW = 1;
static const unsigned int FEATURE_SMOOTHNESS_ALBEDO_ALPHA = 2;
static const unsigned int FEATURE_ALPHA_CLIP = 4;
static const unsigned int FEATURE_EMISSION = 8;
static const unsigned int FEATURE_RECEIVE_SHADOWS = 16;
static const unsigned int FEATURE_SPECULAR_HIGHLIGHTS = 32;
static const unsigned int FEATURE_ENVIRONMENT_REFLECTIONS = 64;
static const unsigned int FEATURE_TRANSPARENT = 128;

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

// Копіює налаштування матеріалу, створюючи копії власний константний буфер та реєстрацію; копія не є файлом матеріалу
Material::Material(const Material& material)
{
	mVertexShader = material.mVertexShader;
	mPixelShader = material.mPixelShader;

	// Текстури належать менеджеру текстур, тому копія користується тими самими
	for (int i = 0; i < (int)MaterialMap::Count; i++)
	{
		mMaps[i] = material.mMaps[i];
	}

	properties = material.properties;
	name = material.name;

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

// Копіює налаштування іншого матеріалу, зберігаючи власний константний буфер, ім'я та файл
Material& Material::operator=(const Material& material)
{
	if (this == &material) return *this;

	mVertexShader = material.mVertexShader;
	mPixelShader = material.mPixelShader;

	for (int i = 0; i < (int)MaterialMap::Count; i++)
	{
		mMaps[i] = material.mMaps[i];
	}

	properties = material.properties;

	// Нульовий та перший слоти лишаються власними, решту можна перенести
	int count = sizeof(mConstantBuffers) / sizeof(mConstantBuffers[0]);

	for (int i = 2; i < count; i++)
	{
		mConstantBuffers[i] = material.mConstantBuffers[i];
	}

	return *this;
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

// Ставить текстуру карти; nullptr прибирає карту
void Material::setMap(MaterialMap map, Texture* texture)
{
	if (map == MaterialMap::Count) return;

	mMaps[(int)map] = texture;
}

// Повертає текстуру карти, або nullptr, якщо її немає
Texture* Material::getMap(MaterialMap map) const
{
	if (map == MaterialMap::Count) return nullptr;

	return mMaps[(int)map];
}

// Встановлює основний колір матеріалу
void Material::setColor(float r, float g, float b, float a)
{
	properties.baseColor[0] = r;
	properties.baseColor[1] = g;
	properties.baseColor[2] = b;
	properties.baseColor[3] = a;
}

// Повертає шлях до піксельного шейдера матеріалу, щоб сцена не втрачала нестандартний шейдер
std::wstring Material::getPixelShaderPath() const
{
	if (mPixelShader == nullptr) return std::wstring();

	return GraphicsEngine::get()->getPixelShaderPath(mPixelShader);
}

// Перевіряє, чи матеріал прозорий і малюється після непрозорих
bool Material::isTransparent() const
{
	return properties.surface == SurfaceType::Transparent;
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
	const MaterialProperties& p = properties;

	for (int i = 0; i < 4; i++)
	{
		materialData.baseColor[i] = p.baseColor[i];
	}

	for (int i = 0; i < 3; i++)
	{
		materialData.specularColor[i] = p.specularColor[i];
		materialData.emissionColor[i] = p.emissionColor[i];
	}

	materialData.tilingOffset[0] = p.tiling[0];
	materialData.tilingOffset[1] = p.tiling[1];
	materialData.tilingOffset[2] = p.offset[0];
	materialData.tilingOffset[3] = p.offset[1];

	materialData.detailTilingOffset[0] = p.detailTiling[0];
	materialData.detailTilingOffset[1] = p.detailTiling[1];
	materialData.detailTilingOffset[2] = p.detailOffset[0];
	materialData.detailTilingOffset[3] = p.detailOffset[1];

	materialData.surface[0] = p.metallic;
	materialData.surface[1] = p.smoothness;
	materialData.surface[2] = p.normalScale;
	materialData.surface[3] = p.heightScale;

	materialData.extra[0] = p.occlusionStrength;
	materialData.extra[1] = p.alphaCutoff;
	materialData.extra[2] = p.detailAlbedoScale;
	materialData.extra[3] = p.detailNormalScale;

	// Карта відблиску залежить від обраного способу: металевість чи колір відблиску
	MaterialMap glossMap = p.workflow == MaterialWorkflow::Metallic ? MaterialMap::Metallic : MaterialMap::Specular;

	unsigned int maps = 0;
	if (getMap(MaterialMap::Base)) maps |= MAP_BASE;
	if (getMap(glossMap)) maps |= MAP_GLOSS;
	if (getMap(MaterialMap::Normal)) maps |= MAP_NORMAL;
	if (getMap(MaterialMap::Height)) maps |= MAP_HEIGHT;
	if (getMap(MaterialMap::Occlusion)) maps |= MAP_OCCLUSION;
	if (getMap(MaterialMap::Emission)) maps |= MAP_EMISSION;
	if (getMap(MaterialMap::DetailMask)) maps |= MAP_DETAIL_MASK;
	if (getMap(MaterialMap::DetailAlbedo)) maps |= MAP_DETAIL_ALBEDO;
	if (getMap(MaterialMap::DetailNormal)) maps |= MAP_DETAIL_NORMAL;

	unsigned int features = 0;
	if (p.workflow == MaterialWorkflow::Specular) features |= FEATURE_SPECULAR_WORKFLOW;
	if (p.smoothnessSource == SmoothnessSource::AlbedoAlpha) features |= FEATURE_SMOOTHNESS_ALBEDO_ALPHA;
	if (p.alphaClipping) features |= FEATURE_ALPHA_CLIP;
	if (p.emission) features |= FEATURE_EMISSION;
	if (p.receiveShadows) features |= FEATURE_RECEIVE_SHADOWS;
	if (p.specularHighlights) features |= FEATURE_SPECULAR_HIGHLIGHTS;
	if (p.environmentReflections) features |= FEATURE_ENVIRONMENT_REFLECTIONS;
	if (p.surface == SurfaceType::Transparent) features |= FEATURE_TRANSPARENT;

	materialData.flags[0] = maps;
	materialData.flags[1] = features;
	materialData.flags[2] = (unsigned int)p.blend;
	materialData.flags[3] = 0;

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
