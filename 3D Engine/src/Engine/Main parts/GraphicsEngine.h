#pragma once
#include <d3d11.h>
#include <d2d1.h>
#include <unordered_map>
#include <string>

class SwapChain;
class DeviceContext;
class VertexBuffer;
class IndexBuffer;
class ConstantBuffer;
class VertexShader;
class PixelShader;
class Texture;
class TextureManager;
class Mesh;
class MeshManager;
class Material;
class GlobalResources;
class ShadowMap;
class PostProcessing;

// Прохід рендеру: спершу непрозорі поверхні, потім прозорі
enum class RenderPass { Opaque, Transparent };

class GraphicsEngine
{
public:
	// Ініціалізує графічний рушій
	bool init();
	// Звільняє ресурси графічного рушія
	bool release();

	// Повертає єдиний екземпляр GraphicsEngine (синглтон)
	static GraphicsEngine* get();

	// Створює новий SwapChain
	SwapChain* createSwapShain();
	// Повертає основний DeviceContext
	DeviceContext* getImmDeviceContext();
	// Створює новий VertexBuffer
	VertexBuffer* createVertexBuffer();
	// Створює новий IndexBuffer
	IndexBuffer* createIndexBuffer();
	// Створює новий ConstantBuffer
	ConstantBuffer* createConstantBuffer();
	// Створює новий VertexShader з байткоду
	VertexShader* createVertexShader(const void* shaderBytecode, SIZE_T bytecodeLength);
	// Створює новий PixelShader з байткоду
	PixelShader* createPixelShader(const void* shaderBytecode, SIZE_T bytecodeLength);
	// Повертає VertexShader за ім'ям файлу та точкою входу
	VertexShader* getVertexShader(const wchar_t* fileName, const char* entryPoint);
	// Повертає PixelShader за ім'ям файлу та точкою входу
	PixelShader* getPixelShader(const wchar_t* fileName, const char* entryPoint);
	// Повертає шлях, з якого завантажено піксельний шейдер, щоб сцена могла його зберегти
	std::wstring getPixelShaderPath(PixelShader* pixelShader) const;

	// Повертає менеджер текстур
	TextureManager* getTextureManager();
	// Повертає менеджер мешів
	MeshManager* getMeshManager();
	// Повертає менеджер глобальних ресурсів
	GlobalResources* getGlobalResources();
	// Повертає карту тіней напрямленого світла
	ShadowMap* getShadowMap();
	// Повертає менеджер постобробки кадру
	PostProcessing* getPostProcessing();

	// Компілює вершинний шейдер з файлу
	bool compileVertexShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength);
	// Звільняє ресурси вершинного шейдера
	void releaseVertexShader();
	// Компілює піксельний шейдер з файлу
	bool compilePixelShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength);
	// Звільняє ресурси піксельного шейдера
	void releasePixelShader();

	// Встановлює матеріал в шейдерах
	void setMaterial(Material* material);
	// Готує матеріал для проходу тіней: обрізання за альфою потребує піксельного шейдера й основної карти
	void setShadowMaterial(Material* material);
	// Повертає стани змішування й глибини до стандартних після прозорих поверхонь
	void resetRenderStates();

	// Встановлює поточний прохід рендеру
	void setRenderPass(RenderPass pass);
	// Повертає поточний прохід рендеру
	RenderPass getRenderPass() const;

	// Ставить небесну текстуру, з якої беруться навколишнє світло й відбиття; nullptr прибирає небо
	void setEnvironmentMap(Texture* texture);
	// Повертає небесну текстуру навколишнього світла
	Texture* getEnvironmentMap() const;
	// Передає шейдерам небо та силу навколишнього світла на цей кадр
	void bindEnvironment();

	// Сила навколишнього світла й відбиттів неба
	float environmentIntensity = 1.0f;
	float reflectionIntensity = 1.0f;

	// Встановлює рівень анізотропної фільтрації текстур (1 - фільтрація вимкнена)
	void setAnisotropy(UINT level);
	// Повертає поточний рівень анізотропної фільтрації текстур
	UINT getAnisotropy();

	// Рендерить сцену в карту тіней з точки зору напрямленого світла
	void renderShadowPass();

	// Виконує рендеринг інтерфейсу користувача
	void renderUI();

private:
	GraphicsEngine();
	~GraphicsEngine();

	// Створює стани растеризатора
	bool createRasterizerStates();
	// Створює стани семплера
	bool createSamplerStates();

	ID3D11Device* mD3dDevice = nullptr;
	D3D_FEATURE_LEVEL mFeatureLevel = {};
	DeviceContext* mImmDeviceContext = nullptr;

	ID3D11RasterizerState* mRasterStateCullFront = nullptr;
	ID3D11RasterizerState* mRasterStateCullBack = nullptr;
	ID3D11RasterizerState* mRasterStateCullNone = nullptr;

	// Стани змішування прозорих матеріалів у порядку BlendMode
	ID3D11BlendState* mBlendStates[4] = {};
	// Прозорі поверхні перевіряють глибину, але не записують її
	ID3D11DepthStencilState* mTransparentDepthState = nullptr;
	// Піксельний шейдер тіней для матеріалів з обрізанням за альфою
	PixelShader* mShadowClipShader = nullptr;

	Texture* mEnvironmentMap = nullptr;
	RenderPass mRenderPass = RenderPass::Opaque;

	ID3D11SamplerState* mSamplerWrap = nullptr;
	ID3D11SamplerState* mSamplerClamp = nullptr;

	UINT mAnisotropy = D3D11_REQ_MAXANISOTROPY;

	IDXGIDevice* mDxgiDevice = nullptr;
	IDXGIAdapter* mDxgiAdapter = nullptr;
	IDXGIFactory* mDxgiFactory = nullptr;

	ID3DBlob* mVSBlob = nullptr;
	ID3DBlob* mPSBlob = nullptr;

	ID3D11VertexShader* mVS = nullptr;
	ID3D11PixelShader* mPS = nullptr;

	TextureManager* mTextureManager = nullptr;
	MeshManager* mMeshManager = nullptr;
	GlobalResources* mGlobalResources = nullptr;
	ShadowMap* mShadowMap = nullptr;
	PostProcessing* mPostProcessing = nullptr;

	std::unordered_map<std::wstring, VertexShader*> vertexShaderMap;
	std::unordered_map<std::wstring, PixelShader*> pixelShaderMap;

	friend class SwapChain;
	friend class DeviceContext;
	friend class VertexBuffer;
	friend class IndexBuffer;
	friend class ConstantBuffer;
	friend class VertexShader;
	friend class PixelShader;
	friend class Texture;
	friend class Mesh;
	friend class ShadowMap;
	friend class PostProcessing;
	friend class EditorGrid;
	friend class SelectionOutline;
	friend class AssetPreview;
};
