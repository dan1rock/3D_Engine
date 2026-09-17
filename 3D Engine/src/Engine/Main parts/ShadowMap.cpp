#include "ShadowMap.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "VertexShader.h"
#include "Matrix.h"

#include <iostream>

ShadowMap::ShadowMap()
{
}

// Ініціалізує карту тіней вказаної роздільності: буфер глибини, семплер, растеризатор та шейдер глибини
bool ShadowMap::init(UINT resolution)
{
	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	mResolution = resolution;

	// Створення текстури глибини, яка водночас є ціллю рендеру та ресурсом для шейдерів
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = mResolution;
	texDesc.Height = mResolution;
	texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.MiscFlags = 0;
	texDesc.ArraySize = 1;
	texDesc.CPUAccessFlags = 0;

	HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &mDepthTexture);

	if (FAILED(hr))
	{
		std::cout << "Failed to create shadow map texture" << std::endl;
		return false;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
	depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	hr = device->CreateDepthStencilView(mDepthTexture, &depthViewDesc, &mDepthStencilView);

	if (FAILED(hr))
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc = {};
	resourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
	resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	resourceViewDesc.Texture2D.MipLevels = 1;

	hr = device->CreateShaderResourceView(mDepthTexture, &resourceViewDesc, &mShaderResourceView);

	if (FAILED(hr))
		return false;

	// Семплер порівняння виконує тест глибини апаратно та згладжує його результат
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// За межами карти тіней глибина вважається максимальною, тому такі пікселі залишаються освітленими
	sampDesc.BorderColor[0] = 1.0f;
	sampDesc.BorderColor[1] = 1.0f;
	sampDesc.BorderColor[2] = 1.0f;
	sampDesc.BorderColor[3] = 1.0f;

	hr = device->CreateSamplerState(&sampDesc, &mComparisonSampler);

	if (FAILED(hr))
		return false;

	// Растеризатор зі зсувом глибини по нахилу, без обрізання по ближній площині та без відкидання граней
	CD3D11_RASTERIZER_DESC rastDesc(D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE,
		0, D3D11_DEFAULT_DEPTH_BIAS_CLAMP,
		2.0f, FALSE, FALSE, FALSE, FALSE);

	hr = device->CreateRasterizerState(&rastDesc, &mRasterState);

	if (FAILED(hr))
		return false;

	mDepthVertexShader = GraphicsEngine::get()->getVertexShader(L"src\\Shaders\\ShadowVertexShader.hlsl", "main");

	if (mDepthVertexShader == nullptr)
		return false;

	return true;
}

// Звільняє ресурси карти тіней
bool ShadowMap::release()
{
	if (mShaderResourceView) mShaderResourceView->Release();
	if (mDepthStencilView) mDepthStencilView->Release();
	if (mDepthTexture) mDepthTexture->Release();
	if (mComparisonSampler) mComparisonSampler->Release();
	if (mRasterState) mRasterState->Release();

	delete this;
	return true;
}

ShadowMap::~ShadowMap()
{
}

// Оновлює матриці світла та параметри тіней у глобальних константах
void ShadowMap::update()
{
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	// Вимкнена карта тіней передає шейдерам нульову силу затінення
	if (!mEnabled || mDepthStencilView == nullptr)
	{
		constantData->shadowParams[3] = 0.0f;
		return;
	}

	updateLightMatrices();

	constantData->shadowParams[0] = 1.0f / (float)mResolution;
	constantData->shadowParams[1] = mDepthBias;
	constantData->shadowParams[2] = mSlopeBias;
	constantData->shadowParams[3] = mStrength;
}

// Встановлює буфер глибини карти тіней як ціль рендеру та готує шейдер глибини
void ShadowMap::begin()
{
	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	// Знімає карту тіней з піксельного шейдера, щоб звільнити її для запису
	deviceContext->setShaderResource(nullptr, 1);

	deviceContext->clearDepthTarget(mDepthStencilView);
	deviceContext->setViewportSize(mResolution, mResolution);
	deviceContext->setRasterizer(mRasterState);

	// Проходу потрібна лише глибина, тому піксельний шейдер не використовується
	deviceContext->setVertexShader(mDepthVertexShader);
	deviceContext->setPixelShader(nullptr);

	deviceContext->setConstantBuffer(mDepthVertexShader, GraphicsEngine::get()->getGlobalResources()->getConstantBuffer(), 0);
}

// Передає заповнену карту тіней піксельним шейдерам
void ShadowMap::end()
{
	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	// Поки буфер глибини залишається ціллю рендеру, читати його в шейдерах не можна
	deviceContext->unsetRenderTargets();

	deviceContext->setShaderResource(mShaderResourceView, 1);
	deviceContext->setSamplerState(mComparisonSampler, 1);
}

// Встановлює розмір області сцени, яку покриває карта тіней, та глибину цієї області
void ShadowMap::setArea(float area, float depthRange)
{
	mArea = area;
	mDepthRange = depthRange;
}

// Встановлює зсуви глибини для боротьби з артефактами самозатінення
void ShadowMap::setBias(float depthBias, float slopeBias)
{
	mDepthBias = depthBias;
	mSlopeBias = slopeBias;
}

// Встановлює силу затінення (0 - тіні не помітні, 1 - повністю затемнені)
void ShadowMap::setStrength(float strength)
{
	mStrength = strength;
}

// Вмикає або вимикає рендеринг тіней
void ShadowMap::setEnabled(bool enabled)
{
	mEnabled = enabled;
}

// Перевіряє, чи увімкнено рендеринг тіней
bool ShadowMap::isEnabled()
{
	return mEnabled && mDepthStencilView != nullptr;
}

// Повертає роздільність карти тіней
UINT ShadowMap::getResolution()
{
	return mResolution;
}

// Обчислює матрицю виду та проекції світла так, щоб область тіней слідувала за камерою
void ShadowMap::updateLightMatrices()
{
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	// Напрямок світла задає компонент DirectionalLight, інакше він виводиться з позиції світла
	Vector3 direction(constantData->lightDir[0], constantData->lightDir[1], constantData->lightDir[2]);

	if (direction.length() < 0.0001f)
	{
		direction = -Vector3(constantData->lightPos[0], constantData->lightPos[1], constantData->lightPos[2]);
	}

	if (direction.length() < 0.0001f)
	{
		direction = Vector3(0.0f, -1.0f, 0.0f);
	}

	direction = direction.normalized();

	constantData->lightDir[0] = direction.x;
	constantData->lightDir[1] = direction.y;
	constantData->lightDir[2] = direction.z;

	// Будує ортонормований базис світла, обираючи опорний вектор так, щоб він не був колінеарним напрямку
	Vector3 reference = fabsf(direction.y) > 0.99f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
	Vector3 right = reference.cross(direction).normalized();
	Vector3 up = direction.cross(right);

	// Центрує область тіней перед камерою, щоб роздільність витрачалася на видиму частину сцени
	Vector3 cameraPos(constantData->cameraPos[0], constantData->cameraPos[1], constantData->cameraPos[2]);
	Vector3 cameraForward(constantData->view.mat[0][2], constantData->view.mat[1][2], constantData->view.mat[2][2]);

	Vector3 center = cameraPos;

	if (cameraForward.length() > 0.0001f)
	{
		center = cameraPos + cameraForward.normalized() * (mArea * 0.3f);
	}

	// Прив'язує центр до сітки текселів, інакше тіні мерехтять під час руху камери
	float texelSize = mArea / (float)mResolution;

	float centerRight = floorf((center * right) / texelSize) * texelSize;
	float centerUp = floorf((center * up) / texelSize) * texelSize;
	float centerForward = center * direction;

	center = right * centerRight + up * centerUp + direction * centerForward;

	// Відносить світло назад проти свого напрямку, щоб уся область тіней потрапила в його проекцію
	Vector3 lightPosition = center - direction * (mDepthRange * 0.5f);

	Matrix lightWorld;
	lightWorld.setIdentity();

	lightWorld.mat[0][0] = right.x; lightWorld.mat[0][1] = right.y; lightWorld.mat[0][2] = right.z;
	lightWorld.mat[1][0] = up.x; lightWorld.mat[1][1] = up.y; lightWorld.mat[1][2] = up.z;
	lightWorld.mat[2][0] = direction.x; lightWorld.mat[2][1] = direction.y; lightWorld.mat[2][2] = direction.z;

	lightWorld.setTranslation(lightPosition);

	Matrix lightView = lightWorld;
	lightView.inverse();

	Matrix lightProjection;
	lightProjection.setOrthoPM(mArea, mArea, 0.1f, mDepthRange);

	constantData->lightViewProjection = lightView * lightProjection;
}
