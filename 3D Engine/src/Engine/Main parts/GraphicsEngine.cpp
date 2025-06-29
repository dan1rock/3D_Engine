#include "GraphicsEngine.h"
#include "SwapChain.h"
#include "DeviceContext.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "TextureManager.h"
#include "MeshManager.h"
#include "GlobalResources.h"
#include "EntityManager.h"
#include "Material.h"
#include "DirectXTex.h"
#include <d3dcompiler.h>
#include "imgui_impl_dx11.h"

#include <iostream>

GraphicsEngine::GraphicsEngine()
{
	try
	{
		mTextureManager = new TextureManager();
		mMeshManager = new MeshManager();
		mGlobalResources = new GlobalResources();
	}
	catch(...) {}
}

// Ініціалізує графічний рушій
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
	
	// Обираємо підтримуваний драйвер та створюємо пристрій
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

	// Отримуємо DXGI Device, Adapter та Factory
	mD3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&mDxgiDevice);
	mDxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&mDxgiAdapter);
	mDxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&mDxgiFactory);

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	// Ініціалізація станів семплера та растеризатора
	createSamplerStates();
	createRasterizerStates();

	// Ініціалізація менеджеру глобальних ресурсів
	mGlobalResources->init();

	// Ініціалізація imgui
	ImGui_ImplDX11_Init(mD3dDevice, mImmContext);

	return true;
}

// Звільняє ресурси графічного рушія
bool GraphicsEngine::release()
{
	mDxgiDevice->Release();
	mDxgiAdapter->Release();
	mDxgiFactory->Release();

	mImmDeviceContext->release();
	mD3dDevice->Release();

	mRasterStateCullFront->Release();
	mRasterStateCullBack->Release();
	mSamplerWrap->Release();
	mSamplerClamp->Release();

	return true;
}

GraphicsEngine::~GraphicsEngine()
{
	delete mTextureManager;
	delete mMeshManager;
	delete mGlobalResources;
}

// Повертає єдиний екземпляр GraphicsEngine (синглтон)
GraphicsEngine* GraphicsEngine::get()
{
	static GraphicsEngine engine;
	return &engine;
}

// Створює новий SwapChain
SwapChain* GraphicsEngine::createSwapShain()
{
	return new SwapChain();
}

// Повертає основний DeviceContext
DeviceContext* GraphicsEngine::getImmDeviceContext()
{
	return this->mImmDeviceContext;
}

// Створює новий VertexBuffer
VertexBuffer* GraphicsEngine::createVertexBuffer()
{
	return new VertexBuffer();
}

// Створює новий IndexBuffer
IndexBuffer* GraphicsEngine::createIndexBuffer()
{
	return new IndexBuffer();
}

// Створює новий ConstantBuffer
ConstantBuffer* GraphicsEngine::createConstantBuffer()
{
	return new ConstantBuffer();
}

// Створює новий VertexShader з байткоду
VertexShader* GraphicsEngine::createVertexShader(const void* shaderBytecode, SIZE_T bytecodeLength)
{
	VertexShader* vs = new VertexShader();
	if (!vs->init(shaderBytecode, bytecodeLength)) {
		vs->release();
		return nullptr;
	};

	return vs;
}

// Створює новий PixelShader з байткоду
PixelShader* GraphicsEngine::createPixelShader(const void* shaderBytecode, SIZE_T bytecodeLength)
{
	PixelShader* ps = new PixelShader();
	if (!ps->init(shaderBytecode, bytecodeLength)) {
		ps->release();
		return nullptr;
	};

	return ps;
}

// Повертає VertexShader за ім'ям файлу та точкою входу
VertexShader* GraphicsEngine::getVertexShader(const wchar_t* fileName, const char* entryPoint)
{
	auto it = vertexShaderMap.find(fileName);

	if (it != vertexShaderMap.end())
		return it->second;

	void* shaderByteCode = nullptr;
	SIZE_T shaderSize = 0;
	VertexShader* shader;

	GraphicsEngine::get()->compileVertexShader(fileName, entryPoint, &shaderByteCode, &shaderSize);
	shader = GraphicsEngine::get()->createVertexShader(shaderByteCode, shaderSize);
	GraphicsEngine::get()->releaseVertexShader();

	if (shader)
	{
		vertexShaderMap[fileName] = shader;
		return shader;
	}

	std::cout << "Failed to load vertex shader: " << fileName << std::endl;

	return nullptr;
}

// Повертає PixelShader за ім'ям файлу та точкою входу
PixelShader* GraphicsEngine::getPixelShader(const wchar_t* fileName, const char* entryPoint)
{
	auto it = pixelShaderMap.find(fileName);

	if (it != pixelShaderMap.end())
		return it->second;

	void* shaderByteCode = nullptr;
	SIZE_T shaderSize = 0;
	PixelShader* shader;

	GraphicsEngine::get()->compilePixelShader(fileName, entryPoint, &shaderByteCode, &shaderSize);
	shader = GraphicsEngine::get()->createPixelShader(shaderByteCode, shaderSize);
	GraphicsEngine::get()->releasePixelShader();

	if (shader)
	{
		pixelShaderMap[fileName] = shader;
		return shader;
	}

	std::cout << "Failed to load pixel shader: " << fileName << std::endl;

	return nullptr;
}

// Повертає менеджер текстур
TextureManager* GraphicsEngine::getTextureManager()
{
	return mTextureManager;
}

// Повертає менеджер мешів
MeshManager* GraphicsEngine::getMeshManager()
{
	return mMeshManager;
}

// Повертає менеджер глобальних ресурсів
GlobalResources* GraphicsEngine::getGlobalResources()
{
	return mGlobalResources;
}

// Компілює вершинний шейдер з файлу
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

// Звільняє ресурси вершинного шейдера
void GraphicsEngine::releaseVertexShader()
{
	if(mVSBlob) mVSBlob->Release();
}

// Компілює піксельний шейдер з файлу
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

// Звільняє ресурси піксельного шейдера
void GraphicsEngine::releasePixelShader()
{
	if (mPSBlob) mPSBlob->Release();
}

// Встановлює матеріал в шейдерах
void GraphicsEngine::setMaterial(Material* material)
{
	material->onMaterialSet();
	mImmDeviceContext->setVertexShader(material->mVertexShader);
	mImmDeviceContext->setPixelShader(material->mPixelShader);
	mImmDeviceContext->setRasterizer(material->cullBack ? mRasterStateCullBack : mRasterStateCullFront);

	if (material->mTextures.size() > 0)
	{
		mImmDeviceContext->setTexture(material->mPixelShader, material->mTextures[0]);

		mImmDeviceContext->setSamplerState(material->clampTexture ? mSamplerClamp : mSamplerWrap);
	}
}

void GraphicsEngine::renderUI()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

// Створює стани растеризатора
bool GraphicsEngine::createRasterizerStates()
{
	CD3D11_RASTERIZER_DESC rastDesc(D3D11_FILL_SOLID, D3D11_CULL_BACK, FALSE,
		D3D11_DEFAULT_DEPTH_BIAS, D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D11_DEFAULT_SLOPE_SCALED_DEPTH_BIAS, TRUE, FALSE, FALSE, TRUE);

	HRESULT hr = GraphicsEngine::get()->mD3dDevice->CreateRasterizerState(&rastDesc, &mRasterStateCullBack);

	if (!SUCCEEDED(hr)) return false;

	rastDesc.CullMode = D3D11_CULL_FRONT;

	hr = GraphicsEngine::get()->mD3dDevice->CreateRasterizerState(&rastDesc, &mRasterStateCullFront);

	if (!SUCCEEDED(hr)) return false;
	return true;
}

// Створює стани семплера
bool GraphicsEngine::createSamplerStates()
{
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	sampDesc.MaxAnisotropy = 16;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = GraphicsEngine::get()->mD3dDevice->CreateSamplerState(&sampDesc, &mSamplerWrap);

	if (FAILED(hr)) return false;

	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

	hr = GraphicsEngine::get()->mD3dDevice->CreateSamplerState(&sampDesc, &mSamplerClamp);

	if (FAILED(hr)) return false;

	return true;
}
