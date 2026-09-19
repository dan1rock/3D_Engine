#include "PostProcessing.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "ConstantBuffer.h"

#include <iostream>

// Параметри ефектів, які передаються у повноекранні шейдери
__declspec(align(16))
struct postProcess {
	float texelSize[4];
	float blurDirection[4];
	// Поріг світіння, м'якість коліна, сила світіння та ознака увімкнення
	float bloomParams[4];
	// Сила віньєтки, плавність, пропорції екрана та ознака увімкнення
	float vignetteParams[4];
	float vignetteColor[4];
};

postProcess postProcessData = {};

// Постобробка працює на власних слотах, бо слоти 0 та 1 під час основного проходу
// зайняті текстурою матеріалу та картою тіней
static const UINT SOURCE_SLOT = 2;
static const UINT BLOOM_SLOT = 3;
static const UINT SAMPLER_SLOT = 2;

PostProcessing::PostProcessing()
{
}

// Завантажує шейдери, створює константний буфер та стан семплера для повноекранних проходів
bool PostProcessing::init()
{
	mFullscreenVertexShader = GraphicsEngine::get()->getVertexShader(L"src\\Shaders\\PostProcessVertexShader.hlsl", "main");
	mExtractPixelShader = GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\BloomExtractPixelShader.hlsl", "main");
	mBlurPixelShader = GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\BloomBlurPixelShader.hlsl", "main");
	mCompositePixelShader = GraphicsEngine::get()->getPixelShader(L"src\\Shaders\\PostProcessPixelShader.hlsl", "main");

	if (mFullscreenVertexShader == nullptr || mExtractPixelShader == nullptr
		|| mBlurPixelShader == nullptr || mCompositePixelShader == nullptr)
	{
		std::cout << "Failed to load post processing shaders" << std::endl;
		return false;
	}

	mConstantBuffer = GraphicsEngine::get()->createConstantBuffer();
	mConstantBuffer->load(&postProcessData, sizeof(postProcessData));

	// Повноекранні проходи читають сусідні пікселі, тому потрібна лінійна фільтрація без повторення текстури
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	HRESULT hr = GraphicsEngine::get()->mD3dDevice->CreateSamplerState(&sampDesc, &mSampler);

	if (FAILED(hr))
		return false;

	// Повноекранний трикутник не має відкидатися за напрямком обходу, яким би не був стан після сцени
	CD3D11_RASTERIZER_DESC rastDesc(D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE,
		0, D3D11_DEFAULT_DEPTH_BIAS_CLAMP, 0.0f, FALSE, FALSE, FALSE, FALSE);

	hr = GraphicsEngine::get()->mD3dDevice->CreateRasterizerState(&rastDesc, &mRasterState);

	if (FAILED(hr))
		return false;

	return true;
}

// Звільняє ресурси постобробки
bool PostProcessing::release()
{
	releaseRenderTexture(mSceneTexture);
	releaseRenderTexture(mBloomTextures[0]);
	releaseRenderTexture(mBloomTextures[1]);

	if (mRasterState) mRasterState->Release();
	if (mSampler) mSampler->Release();
	if (mConstantBuffer) mConstantBuffer->release();

	delete this;
	return true;
}

PostProcessing::~PostProcessing()
{
}

// Перестворює проміжні цілі рендеру під новий розмір вікна
void PostProcessing::resize(UINT width, UINT height)
{
	if (width == 0 || height == 0) return;

	if (mSceneTexture.width == width && mSceneTexture.height == height) return;

	releaseRenderTexture(mSceneTexture);
	releaseRenderTexture(mBloomTextures[0]);
	releaseRenderTexture(mBloomTextures[1]);

	// Сцена зберігається у форматі з плаваючою комою, щоб яскравість понад одиницю не обрізалась до світіння
	if (!createRenderTexture(mSceneTexture, width, height))
	{
		std::cout << "Failed to create post processing scene target" << std::endl;
		return;
	}

	// Світіння готується у половинному розділенні: розмиття однаково широке, але вчетверо дешевше
	UINT bloomWidth = width / 2 > 0 ? width / 2 : 1;
	UINT bloomHeight = height / 2 > 0 ? height / 2 : 1;

	createRenderTexture(mBloomTextures[0], bloomWidth, bloomHeight);
	createRenderTexture(mBloomTextures[1], bloomWidth, bloomHeight);
}

// Спрямовує рендеринг сцени у проміжну текстуру замість вікна
void PostProcessing::begin(SwapChain* swapChain)
{
	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	// Без постобробки сцена малюється одразу у вікно
	if (!isEnabled())
	{
		deviceContext->clearRenderTarget(swapChain, 0.1f, 0.1f, 0.1f, 1);
		return;
	}

	// Текстури минулого кадру ще прив'язані до піксельного шейдера і не можуть бути ціллю рендеру
	deviceContext->setShaderResource(nullptr, SOURCE_SLOT);
	deviceContext->setShaderResource(nullptr, BLOOM_SLOT);

	deviceContext->clearRenderTarget(mSceneTexture.renderTargetView, swapChain, 0.1f, 0.1f, 0.1f, 1);
}

// Застосовує ефекти до відрендереної сцени та виводить результат у вікно
void PostProcessing::apply(SwapChain* swapChain)
{
	if (!isEnabled()) return;

	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	// Глибина більше не потрібна, а сцену треба звільнити для читання в шейдерах
	deviceContext->unsetRenderTargets();

	// Проходи постобробки не залежать від станів, які лишила по собі сцена
	deviceContext->setRasterizer(mRasterState);
	deviceContext->setVertexShader(mFullscreenVertexShader);
	deviceContext->setSamplerState(mSampler, SAMPLER_SLOT);
	deviceContext->setConstantBuffer(mCompositePixelShader, mConstantBuffer, 0);
	deviceContext->setFullscreenTriangle();

	if (mBloomEnabled) renderBloom();

	// Підсумковий прохід поєднує сцену зі світінням та затемнює краї кадру
	// Буфер глибини не потрібен, інакше глибина сцени впливала б на повноекранний трикутник
	deviceContext->setRenderTarget(swapChain, false);
	deviceContext->setViewportSize(mSceneTexture.width, mSceneTexture.height);

	updateConstantBuffer(mSceneTexture);

	deviceContext->setShaderResource(mSceneTexture.shaderResourceView, SOURCE_SLOT);
	deviceContext->setShaderResource(mBloomEnabled ? mBloomTextures[0].shaderResourceView : nullptr, BLOOM_SLOT);
	deviceContext->setPixelShader(mCompositePixelShader);
	deviceContext->drawTriangleList(3, 0);

	// Інтерфейс малюється поверх результату, тому звільняємо слоти від текстур постобробки
	deviceContext->setShaderResource(nullptr, SOURCE_SLOT);
	deviceContext->setShaderResource(nullptr, BLOOM_SLOT);
}

// Готує текстуру світіння: виділяє яскраві місця та розмиває їх
void PostProcessing::renderBloom()
{
	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	if (mBloomTextures[0].renderTargetView == nullptr) return;

	// Виділяємо з кадру лише те, що яскравіше за поріг
	deviceContext->setRenderTarget(mBloomTextures[0].renderTargetView);
	deviceContext->setViewportSize(mBloomTextures[0].width, mBloomTextures[0].height);

	drawFullscreen(mSceneTexture, mExtractPixelShader);

	// Кожен прохід розмиває по черзі по горизонталі та вертикалі, подвоюючи крок
	for (int pass = 0; pass < mBlurPasses; pass++)
	{
		float step = (float)(1 << pass);

		mBlurDirection[0] = 1.0f;
		mBlurDirection[1] = 0.0f;
		mBlurDirection[2] = step;

		deviceContext->setRenderTarget(mBloomTextures[1].renderTargetView);
		drawFullscreen(mBloomTextures[0], mBlurPixelShader);

		mBlurDirection[0] = 0.0f;
		mBlurDirection[1] = 1.0f;
		mBlurDirection[2] = step;

		deviceContext->setRenderTarget(mBloomTextures[0].renderTargetView);
		drawFullscreen(mBloomTextures[1], mBlurPixelShader);
	}
}

// Готує конвеєр до повноекранного проходу та малює трикутник на весь екран
void PostProcessing::drawFullscreen(const RenderTexture& source, PixelShader* pixelShader)
{
	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	updateConstantBuffer(source);

	deviceContext->setShaderResource(nullptr, SOURCE_SLOT);
	deviceContext->setConstantBuffer(pixelShader, mConstantBuffer, 0);
	deviceContext->setPixelShader(pixelShader);
	deviceContext->setShaderResource(source.shaderResourceView, SOURCE_SLOT);
	deviceContext->drawTriangleList(3, 0);

	// Наступним проходом ця текстура може стати ціллю рендеру, тому знімаємо її з шейдера
	deviceContext->setShaderResource(nullptr, SOURCE_SLOT);
}

// Оновлює константний буфер параметрами ефектів
void PostProcessing::updateConstantBuffer(const RenderTexture& source)
{
	postProcessData.texelSize[0] = source.width > 0 ? 1.0f / (float)source.width : 0.0f;
	postProcessData.texelSize[1] = source.height > 0 ? 1.0f / (float)source.height : 0.0f;
	postProcessData.texelSize[2] = (float)source.width;
	postProcessData.texelSize[3] = (float)source.height;

	postProcessData.blurDirection[0] = mBlurDirection[0];
	postProcessData.blurDirection[1] = mBlurDirection[1];
	postProcessData.blurDirection[2] = mBlurDirection[2];

	postProcessData.bloomParams[0] = mBloomThreshold;
	postProcessData.bloomParams[1] = mBloomKnee;
	postProcessData.bloomParams[2] = mBloomIntensity;
	postProcessData.bloomParams[3] = mBloomEnabled ? 1.0f : 0.0f;

	postProcessData.vignetteParams[0] = mVignetteEnabled ? mVignetteIntensity : 0.0f;
	postProcessData.vignetteParams[1] = mVignetteSmoothness;
	postProcessData.vignetteParams[2] = mSceneTexture.height > 0
		? (float)mSceneTexture.width / (float)mSceneTexture.height
		: 1.0f;

	postProcessData.vignetteColor[0] = mVignetteColor[0];
	postProcessData.vignetteColor[1] = mVignetteColor[1];
	postProcessData.vignetteColor[2] = mVignetteColor[2];
	postProcessData.vignetteColor[3] = mVignetteColor[3];

	mConstantBuffer->update(GraphicsEngine::get()->getImmDeviceContext(), &postProcessData);
}

// Створює проміжну ціль рендеру вказаного розміру
bool PostProcessing::createRenderTexture(RenderTexture& target, UINT width, UINT height)
{
	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.MiscFlags = 0;
	texDesc.ArraySize = 1;
	texDesc.CPUAccessFlags = 0;

	if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &target.texture)))
		return false;

	if (FAILED(device->CreateRenderTargetView(target.texture, nullptr, &target.renderTargetView)))
		return false;

	if (FAILED(device->CreateShaderResourceView(target.texture, nullptr, &target.shaderResourceView)))
		return false;

	target.width = width;
	target.height = height;

	return true;
}

// Звільняє проміжну ціль рендеру
void PostProcessing::releaseRenderTexture(RenderTexture& target)
{
	if (target.shaderResourceView) target.shaderResourceView->Release();
	if (target.renderTargetView) target.renderTargetView->Release();
	if (target.texture) target.texture->Release();

	target = {};
}

// Вмикає або вимикає постобробку повністю
void PostProcessing::setEnabled(bool enabled)
{
	mEnabled = enabled;
}

// Перевіряє, чи увімкнено постобробку
bool PostProcessing::isEnabled()
{
	return mEnabled && mSceneTexture.renderTargetView != nullptr;
}

// Вмикає або вимикає ефект світіння
void PostProcessing::setBloomEnabled(bool enabled)
{
	mBloomEnabled = enabled;
}

// Перевіряє, чи увімкнено ефект світіння
bool PostProcessing::isBloomEnabled()
{
	return mBloomEnabled;
}

// Встановлює яскравість, з якої починається світіння, та м'якість переходу до нього
void PostProcessing::setBloomThreshold(float threshold, float knee)
{
	mBloomThreshold = threshold;
	mBloomKnee = knee;
}

// Встановлює силу світіння
void PostProcessing::setBloomIntensity(float intensity)
{
	mBloomIntensity = intensity;
}

// Вмикає або вимикає ефект віньєтки
void PostProcessing::setVignetteEnabled(bool enabled)
{
	mVignetteEnabled = enabled;
}

// Перевіряє, чи увімкнено ефект віньєтки
bool PostProcessing::isVignetteEnabled()
{
	return mVignetteEnabled;
}

// Встановлює силу затемнення країв кадру та плавність його переходу
void PostProcessing::setVignette(float intensity, float smoothness)
{
	mVignetteIntensity = intensity;
	mVignetteSmoothness = smoothness;
}

// Встановлює колір, у який затемнюються краї кадру
void PostProcessing::setVignetteColor(float r, float g, float b)
{
	mVignetteColor[0] = r;
	mVignetteColor[1] = g;
	mVignetteColor[2] = b;
}
