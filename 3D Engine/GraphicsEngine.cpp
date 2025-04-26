#include "GraphicsEngine.h"
#include "SwapChain.h"
#include "DeviceContext.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "DirectXTex.h"
#include <d3dcompiler.h>


GraphicsEngine::GraphicsEngine()
{
	try
	{
		mTextureManager = new TextureManager();
	}
	catch(...) {}
}

bool GraphicsEngine::init()
{
	D3D_DRIVER_TYPE driverTypes[] = {
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE
	};

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_11_0
	};

	ID3D11DeviceContext* mImmContext = NULL;
	
	HRESULT create = 0;
	for (UINT index = 0; index < ARRAYSIZE(driverTypes);) {
		create = D3D11CreateDevice(NULL, driverTypes[index], NULL, NULL, featureLevels,
			ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &mD3dDevice, &mFeatureLevel, &mImmContext);
		if (SUCCEEDED(create)) break;

		++index;
	}
	if (FAILED(create))
		return false;

	mImmDeviceContext = new DeviceContext(mImmContext);

	mD3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&mDxgiDevice);
	mDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&mDxgiAdapter);
	mDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&mDxgiFactory);

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	return true;
}

bool GraphicsEngine::release()
{
	mDxgiDevice->Release();
	mDxgiAdapter->Release();
	mDxgiFactory->Release();

	mImmDeviceContext->release();
	mD3dDevice->Release();
	return true;
}

GraphicsEngine::~GraphicsEngine()
{
	delete mTextureManager;
}

GraphicsEngine* GraphicsEngine::get()
{
	static GraphicsEngine engine;
	return &engine;
}

SwapChain* GraphicsEngine::createSwapShain()
{
	return new SwapChain();
}

DeviceContext* GraphicsEngine::getImmDeviceContext()
{
	return this->mImmDeviceContext;
}

VertexBuffer* GraphicsEngine::createVertexBuffer()
{
	return new VertexBuffer();
}

IndexBuffer* GraphicsEngine::createIndexBuffer()
{
	return new IndexBuffer();
}

ConstantBuffer* GraphicsEngine::createConstantBuffer()
{
	return new ConstantBuffer();
}

VertexShader* GraphicsEngine::createVertexShader(const void* shaderBytecode, SIZE_T bytecodeLength)
{
	VertexShader* vs = new VertexShader();
	if (!vs->init(shaderBytecode, bytecodeLength)) {
		vs->release();
		return nullptr;
	};

	return vs;
}

PixelShader* GraphicsEngine::createPixelShader(const void* shaderBytecode, SIZE_T bytecodeLength)
{
	PixelShader* ps = new PixelShader();
	if (!ps->init(shaderBytecode, bytecodeLength)) {
		ps->release();
		return nullptr;
	};

	return ps;
}

TextureManager* GraphicsEngine::getTextureManager()
{
	return mTextureManager;
}

bool GraphicsEngine::compileVertexShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength)
{
	ID3DBlob* errblob = nullptr;
	if (FAILED(D3DCompileFromFile(fileName, nullptr, nullptr, entryPoint, "vs_5_0", 0, 0, &mVSBlob, &errblob))) {
		if(errblob) errblob->Release();
		return false;
	};

	*shaderBytecode = mVSBlob->GetBufferPointer();
	*bytecodeLength = mVSBlob->GetBufferSize();

	return true;
}

void GraphicsEngine::releaseVertexShader()
{
	if(mVSBlob) mVSBlob->Release();
}

bool GraphicsEngine::compilePixelShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength)
{
	ID3DBlob* errblob = nullptr;
	if (FAILED(D3DCompileFromFile(fileName, nullptr, nullptr, entryPoint, "ps_5_0", 0, 0, &mPSBlob, &errblob))) {
		if (errblob) errblob->Release();
		return false;
	};

	*shaderBytecode = mPSBlob->GetBufferPointer();
	*bytecodeLength = mPSBlob->GetBufferSize();

	return true;
}

void GraphicsEngine::releasePixelShader()
{
	if (mPSBlob) mPSBlob->Release();
}

ID3D11RasterizerState* GraphicsEngine::createRasterizer()
{
	CD3D11_RASTERIZER_DESC rastDesc(D3D11_FILL_SOLID, D3D11_CULL_FRONT, FALSE,
		D3D11_DEFAULT_DEPTH_BIAS, D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS, TRUE, FALSE, FALSE, TRUE);

	ID3D11RasterizerState* rasterState;
	HRESULT hr = GraphicsEngine::get()->mD3dDevice->CreateRasterizerState(&rastDesc, &rasterState);

	if (SUCCEEDED(hr))
		return rasterState;

	return nullptr;
}
