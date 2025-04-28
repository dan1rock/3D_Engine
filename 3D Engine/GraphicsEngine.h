#pragma once
#include <d3d11.h>
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

class GraphicsEngine
{
public:
	GraphicsEngine();
	bool init();
	bool release();
	~GraphicsEngine();

	static GraphicsEngine* get();

	SwapChain* createSwapShain();
	DeviceContext* getImmDeviceContext();
	VertexBuffer* createVertexBuffer();
	IndexBuffer* createIndexBuffer();
	ConstantBuffer* createConstantBuffer();
	VertexShader* createVertexShader(const void* shaderBytecode, SIZE_T bytecodeLength);
	PixelShader* createPixelShader(const void* shaderBytecode, SIZE_T bytecodeLength);
	VertexShader* getVertexShader(const wchar_t* fileName, const char* entryPoint);
	PixelShader* getPixelShader(const wchar_t* fileName, const char* entryPoint);

	TextureManager* getTextureManager();
	MeshManager* getMeshManager();

	bool compileVertexShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength);
	void releaseVertexShader();
	bool compilePixelShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength);
	void releasePixelShader();

	ID3D11RasterizerState* createRasterizer();
private:
	ID3D11Device* mD3dDevice = nullptr;
	D3D_FEATURE_LEVEL mFeatureLevel = {};
	DeviceContext* mImmDeviceContext = nullptr;

	IDXGIDevice* mDxgiDevice = nullptr;
	IDXGIAdapter* mDxgiAdapter = nullptr;
	IDXGIFactory* mDxgiFactory = nullptr;

	ID3DBlob* mVSBlob = nullptr;
	ID3DBlob* mPSBlob = nullptr;

	ID3D11VertexShader* mVS = nullptr;
	ID3D11PixelShader* mPS = nullptr;

	TextureManager* mTextureManager = nullptr;
	MeshManager* mMeshManager = nullptr;

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
