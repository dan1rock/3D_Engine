#pragma once
#include <d3d11.h>

class SwapChain;
class DeviceContext;
class VertexBuffer;
class IndexBuffer;
class ConstantBuffer;
class VertexShader;
class PixelShader;
class Texture;

class GraphicsEngine
{
public:
	GraphicsEngine();
	bool init();
	bool release();
	~GraphicsEngine();

	static GraphicsEngine* engine();

	SwapChain* createSwapShain();
	DeviceContext* getImmDeviceContext();
	VertexBuffer* createVertexBuffer();
	IndexBuffer* createIndexBuffer();
	ConstantBuffer* createConstantBuffer();
	VertexShader* createVertexShader(const void* shaderBytecode, SIZE_T bytecodeLength);
	PixelShader* createPixelShader(const void* shaderBytecode, SIZE_T bytecodeLength);

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

	friend class SwapChain;
	friend class DeviceContext;
	friend class VertexBuffer;
	friend class IndexBuffer;
	friend class ConstantBuffer;
	friend class VertexShader;
	friend class PixelShader;
	friend class Texture;
};
