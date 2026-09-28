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
#include "ShadowMap.h"
#include "Frustum.h"
#include "PostProcessing.h"
#include "EntityManager.h"
#include "Material.h"
#include "Texture.h"
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

	mShadowClipShader = getPixelShader(L"src\\Shaders\\ShadowPixelShader.hlsl", "main");

	// Ініціалізація карти тіней напрямленого світла
	mShadowMap = new ShadowMap();
	if (!mShadowMap->init(2048))
	{
		std::cout << "Failed to initialize shadow map" << std::endl;
		mShadowMap->setEnabled(false);
	}

	// Ініціалізація постобробки кадру
	mPostProcessing = new PostProcessing();
	if (!mPostProcessing->init())
	{
		std::cout << "Failed to initialize post processing" << std::endl;
		mPostProcessing->setEnabled(false);
	}

	// Ініціалізація imgui
	ImGui_ImplDX11_Init(mD3dDevice, mImmContext);

	return true;
}

// Звільняє ресурси графічного рушія
bool GraphicsEngine::release()
{
	if (mShadowMap) {
		mShadowMap->release();
		mShadowMap = nullptr;
	}

	if (mPostProcessing) {
		mPostProcessing->release();
		mPostProcessing = nullptr;
	}

	mDxgiDevice->Release();
	mDxgiAdapter->Release();
	mDxgiFactory->Release();

	mImmDeviceContext->release();
	mD3dDevice->Release();

	mRasterStateCullFront->Release();
	mRasterStateCullBack->Release();
	mRasterStateCullNone->Release();

	for (ID3D11BlendState* state : mBlendStates)
	{
		if (state) state->Release();
	}

	if (mTransparentDepthState) mTransparentDepthState->Release();
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

// Повертає шлях, з якого завантажено піксельний шейдер, щоб сцена могла його зберегти
std::wstring GraphicsEngine::getPixelShaderPath(PixelShader* pixelShader) const
{
	// Шейдери кешуються за шляхом, тому зворотний пошук по тому ж словнику дає потрібне ім'я файлу
	for (const auto& entry : pixelShaderMap)
	{
		if (entry.second == pixelShader) return entry.first;
	}

	return std::wstring();
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

// Повертає карту тіней напрямленого світла
ShadowMap* GraphicsEngine::getShadowMap()
{
	return mShadowMap;
}

// Повертає менеджер постобробки кадру
PostProcessing* GraphicsEngine::getPostProcessing()
{
	return mPostProcessing;
}

// Компілює вершинний шейдер з файлу
bool GraphicsEngine::compileVertexShader(const wchar_t* fileName, const char* entryPoint, void** shaderBytecode, SIZE_T* bytecodeLength)
{
	ID3DBlob* errblob = nullptr;
	if (FAILED(D3DCompileFromFile(fileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, "vs_5_0", 0, 0, &mVSBlob, &errblob))) {
		if (errblob) std::cout << (const char*)errblob->GetBufferPointer() << std::endl;
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
	if (FAILED(D3DCompileFromFile(fileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, "ps_5_0", 0, 0, &mPSBlob, &errblob))) {
		if (errblob) std::cout << (const char*)errblob->GetBufferPointer() << std::endl;
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

	const MaterialProperties& properties = material->properties;

	// Передній бік малюється з відкиданням задніх граней, задній - навпаки, обидва - без відкидання
	if (properties.renderFace == RenderFace::Front) mImmDeviceContext->setRasterizer(mRasterStateCullBack);
	else if (properties.renderFace == RenderFace::Back) mImmDeviceContext->setRasterizer(mRasterStateCullFront);
	else mImmDeviceContext->setRasterizer(mRasterStateCullNone);

	// Прозорі змішуються з кадром і не пишуть глибину; непрозорі працюють зі стандартними станами
	if (material->isTransparent())
	{
		mImmDeviceContext->setBlendState(mBlendStates[(int)properties.blend]);
		mImmDeviceContext->setDepthStencilState(mTransparentDepthState);
	}
	else
	{
		mImmDeviceContext->setBlendState(nullptr);
		mImmDeviceContext->setDepthStencilState(nullptr);
	}

	// Слот 1 зайнятий картою тіней, тож карти матеріалу йдуть у слот 0 і далі з другого
	MaterialMap gloss = properties.workflow == MaterialWorkflow::Metallic ? MaterialMap::Metallic : MaterialMap::Specular;

	mImmDeviceContext->setTexture(material->getMap(MaterialMap::Base), 0);
	mImmDeviceContext->setTexture(material->getMap(gloss), 2);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::Normal), 3);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::Height), 4);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::Occlusion), 5);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::Emission), 6);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::DetailMask), 7);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::DetailAlbedo), 8);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::DetailNormal), 9);

	mImmDeviceContext->setSamplerState(mSamplerWrap);
}

// Готує матеріал для проходу тіней: обрізання за альфою потребує піксельного шейдера й основної карти
void GraphicsEngine::setShadowMaterial(Material* material)
{
	if (material == nullptr || !material->properties.alphaClipping || mShadowClipShader == nullptr)
	{
		mImmDeviceContext->setPixelShader(nullptr);
		return;
	}

	material->onMaterialSet();

	mImmDeviceContext->setPixelShader(mShadowClipShader);
	mImmDeviceContext->setTexture(material->getMap(MaterialMap::Base), 0);
	mImmDeviceContext->setSamplerState(mSamplerWrap);
}

// Повертає стани змішування й глибини до стандартних після прозорих поверхонь
void GraphicsEngine::resetRenderStates()
{
	mImmDeviceContext->setBlendState(nullptr);
	mImmDeviceContext->setDepthStencilState(nullptr);
}

// Встановлює поточний прохід рендеру
void GraphicsEngine::setRenderPass(RenderPass pass)
{
	mRenderPass = pass;
}

// Повертає поточний прохід рендеру
RenderPass GraphicsEngine::getRenderPass() const
{
	return mRenderPass;
}

// Ставить небесну текстуру, з якої беруться навколишнє світло й відбиття; nullptr прибирає небо
void GraphicsEngine::setEnvironmentMap(Texture* texture)
{
	mEnvironmentMap = texture;
}

// Повертає небесну текстуру навколишнього світла
Texture* GraphicsEngine::getEnvironmentMap() const
{
	return mEnvironmentMap;
}

// Передає шейдерам небо та силу навколишнього світла на цей кадр
void GraphicsEngine::bindEnvironment()
{
	constant* constantData = mGlobalResources->getConstantData();

	constantData->environmentParams[0] = environmentIntensity;
	constantData->environmentParams[1] = reflectionIntensity;
	constantData->environmentParams[2] = mEnvironmentMap ? (float)mEnvironmentMap->getMipCount() : 0.0f;
	constantData->environmentParams[3] = mEnvironmentMap ? 1.0f : 0.0f;

	// Без неба навколишнє світло має сталий колір
	constantData->ambientColor[0] = 0.0368f;
	constantData->ambientColor[1] = 0.0423f;
	constantData->ambientColor[2] = 0.0544f;
	constantData->ambientColor[3] = 1.0f;

	mImmDeviceContext->setTexture(mEnvironmentMap, 10);
}

// Встановлює рівень анізотропної фільтрації текстур (1 - фільтрація вимкнена)
void GraphicsEngine::setAnisotropy(UINT level)
{
	// Direct3D 11 підтримує щонайбільше шістнадцятикратну анізотропію
	if (level < 1) level = 1;
	if (level > D3D11_REQ_MAXANISOTROPY) level = D3D11_REQ_MAXANISOTROPY;

	if (level == mAnisotropy) return;

	mAnisotropy = level;

	createSamplerStates();
}

// Повертає поточний рівень анізотропної фільтрації текстур
UINT GraphicsEngine::getAnisotropy()
{
	return mAnisotropy;
}

// Рендерить сцену в карту тіней з точки зору напрямленого світла
void GraphicsEngine::renderShadowPass()
{
	if (mShadowMap == nullptr) return;

	// Параметри тіней оновлюються навіть для вимкненої карти, щоб шейдери знали про це
	mShadowMap->update();

	if (!mShadowMap->isEnabled()) return;

	mShadowMap->begin();

	// Кожен каскад отримує власний прохід глибини по всій сцені
	for (int cascade = 0; cascade < mShadowMap->getCascadeCount(); cascade++)
	{
		mShadowMap->beginCascade(cascade);

		// Кожен каскад відсікає об'єкти за власною пірамідою, тому далекі меші
		// не потрапляють у проходи ближніх каскадів
		EntityManager::get()->renderShadowCasters(mShadowMap->getCascadeFrustum(cascade));
	}

	mShadowMap->end();
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

	rastDesc.CullMode = D3D11_CULL_NONE;

	hr = GraphicsEngine::get()->mD3dDevice->CreateRasterizerState(&rastDesc, &mRasterStateCullNone);

	if (!SUCCEEDED(hr)) return false;

	// Способи змішування прозорих поверхонь для кожного режиму змішування матеріалу
	D3D11_BLEND sources[4] = { D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_DEST_COLOR };
	D3D11_BLEND destinations[4] = { D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_ONE, D3D11_BLEND_ZERO };

	for (int i = 0; i < 4; i++)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = sources[i];
		blendDesc.RenderTarget[0].DestBlend = destinations[i];
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		hr = mD3dDevice->CreateBlendState(&blendDesc, &mBlendStates[i]);

		if (!SUCCEEDED(hr)) return false;
	}

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS;

	hr = mD3dDevice->CreateDepthStencilState(&depthDesc, &mTransparentDepthState);

	if (!SUCCEEDED(hr)) return false;

	return true;
}

// Створює стани семплера
bool GraphicsEngine::createSamplerStates()
{
	// Стани перестворюються при зміні рівня фільтрації, тому спершу звільняємо попередні
	if (mSamplerWrap)
	{
		mSamplerWrap->Release();
		mSamplerWrap = nullptr;
	}
	if (mSamplerClamp)
	{
		mSamplerClamp->Release();
		mSamplerClamp = nullptr;
	}

	D3D11_SAMPLER_DESC sampDesc = {};

	// Без анізотропії залишається звичайна трилінійна фільтрація
	sampDesc.Filter = mAnisotropy > 1 ? D3D11_FILTER_ANISOTROPIC : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.MaxAnisotropy = mAnisotropy;
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
