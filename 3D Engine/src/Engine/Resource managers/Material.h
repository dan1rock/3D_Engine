#pragma once
#include <vector>
#include <string>

class PixelShader;
class VertexShader;
class Texture;
class ConstantBuffer;

// Карти матеріалу
enum class MaterialMap
{
	Base,
	Metallic,
	Specular,
	Normal,
	Height,
	Occlusion,
	Emission,
	DetailMask,
	DetailAlbedo,
	DetailNormal,
	Count
};

// Звідки береться металевість і колір відблиску: з металевості чи з окремого кольору відблиску
enum class MaterialWorkflow { Metallic, Specular };
// Непрозора поверхня пише глибину, прозора змішується з тим, що вже намальовано
enum class SurfaceType { Opaque, Transparent };
// Як прозора поверхня змішується з кадром
enum class BlendMode { Alpha, Premultiply, Additive, Multiply };
// Які боки трикутників малюються
enum class RenderFace { Front, Back, Both };
// Звідки береться гладкість: з альфи карти металевості чи з альфи основної карти
enum class SmoothnessSource { MetallicAlpha, AlbedoAlpha };

// Числові налаштування матеріалу з їхніми типовими значеннями
struct MaterialProperties
{
	MaterialWorkflow workflow = MaterialWorkflow::Metallic;
	SurfaceType surface = SurfaceType::Opaque;
	BlendMode blend = BlendMode::Alpha;
	RenderFace renderFace = RenderFace::Front;
	bool alphaClipping = false;
	float alphaCutoff = 0.5f;
	bool receiveShadows = true;

	float baseColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float metallic = 0.0f;
	float smoothness = 0.5f;
	SmoothnessSource smoothnessSource = SmoothnessSource::MetallicAlpha;
	float specularColor[3] = { 0.2f, 0.2f, 0.2f };
	float normalScale = 1.0f;
	float heightScale = 0.005f;
	float occlusionStrength = 1.0f;
	bool emission = false;
	float emissionColor[3] = { 0.0f, 0.0f, 0.0f };

	float tiling[2] = { 1.0f, 1.0f };
	float offset[2] = { 0.0f, 0.0f };

	float detailAlbedoScale = 1.0f;
	float detailNormalScale = 1.0f;
	float detailTiling[2] = { 1.0f, 1.0f };
	float detailOffset[2] = { 0.0f, 0.0f };

	bool specularHighlights = true;
	bool environmentReflections = true;
	int sortingPriority = 0;
};

class Material
{
public:
	// Ініціалізує шейдери, константні буфери та реєструє матеріал
	Material();
	// Копіює налаштування матеріалу, створюючи копії власний константний буфер та реєстрацію; копія не є файлом матеріалу
	Material(const Material& material);
	// Копіює налаштування іншого матеріалу, зберігаючи власний константний буфер, ім'я та файл
	Material& operator=(const Material& material);
	// Звільняє ресурси та знімає реєстрацію матеріалу
	~Material();

	// Встановлює вершинний шейдер для матеріалу
	void setVertexShader(VertexShader* vertexShader);
	// Встановлює піксельний шейдер для матеріалу
	void setPixelShader(PixelShader* pixelShader);

	// Ставить текстуру карти; nullptr прибирає карту
	void setMap(MaterialMap map, Texture* texture);
	// Повертає текстуру карти, або nullptr, якщо її немає
	Texture* getMap(MaterialMap map) const;

	// Встановлює основний колір матеріалу
	void setColor(float r, float g, float b, float a);

	// Повертає шлях до піксельного шейдера матеріалу, щоб сцена не втрачала нестандартний шейдер
	std::wstring getPixelShaderPath() const;

	// Перевіряє, чи матеріал прозорий і малюється після непрозорих
	bool isTransparent() const;

	// Налаштування матеріалу, які показує інспектор
	MaterialProperties properties;

	// Ім'я, яке показує редактор: ім'я файлу матеріалу, або з позначкою копії для режиму гри
	std::string name;
	// Шлях до файлу матеріалу; порожній у матеріалу, створеного кодом, і в копій для режиму гри
	std::string assetPath;

	bool dontDeleteOnLoad = false;

	// Встановлює константний буфер для матеріалу у відповідний слот
	void setConstantBuffer(ConstantBuffer* constantBuffer, int slot = 0);

private:
	// Оновлює дані матеріалу та встановлює константні буфери для шейдерів
	void onMaterialSet();

	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;
	ConstantBuffer* mConstantBuffer = nullptr;

	ConstantBuffer* mConstantBuffers[4] = {};

	// Текстури карт; належать менеджеру текстур, тож копії матеріалу користуються тими самими
	Texture* mMaps[(int)MaterialMap::Count] = {};

	friend class GraphicsEngine;
};
