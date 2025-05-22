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

class GraphicsEngine
{
public:
	GraphicsEngine();
	// Ініціалізує графічний рушій
	bool init();
	// Звільняє ресурси графічного рушія
	bool release();
	~GraphicsEngine();

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

	// Повертає менеджер текстур
	TextureManager* getTextureManager();
	// Повертає менеджер мешів
	MeshManager* getMeshManager();
	// Повертає менеджер глобальних ресурсів
	GlobalResources* getGlobalResources();

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

private:
	// Створює стани растеризатора
	bool createRasterizerStates();
	// Створює стани семплера
	bool createSamplerStates();

	ID3D11Device* mD3dDevice = nullptr;
	D3D_FEATURE_LEVEL mFeatureLevel = {};
	DeviceContext* mImmDeviceContext = nullptr;

	ID3D11RasterizerState* mRasterStateCullFront = nullptr;
	ID3D11RasterizerState* mRasterStateCullBack = nullptr;

	ID3D11SamplerState* mSamplerWrap = nullptr;
	ID3D11SamplerState* mSamplerClamp = nullptr;

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
};
