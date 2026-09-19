#include "ShadowMap.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "VertexShader.h"
#include "Matrix.h"
#include "Frustum.h"

#include <iostream>

ShadowMap::ShadowMap()
{
}

// Ініціалізує карту тіней вказаної роздільності: масив буферів глибини, семплер, растеризатор та шейдер глибини
bool ShadowMap::init(UINT resolution)
{
	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	mResolution = resolution;

	// Усі каскади живуть в одному масиві текстур, тому шейдер читає їх одним ресурсом
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
	texDesc.ArraySize = SHADOW_CASCADE_COUNT;
	texDesc.CPUAccessFlags = 0;

	HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &mDepthTexture);

	if (FAILED(hr))
	{
		std::cout << "Failed to create shadow map texture" << std::endl;
		return false;
	}

	// Кожен каскад рендериться у власний зріз масиву, тому потребує окремого перегляду глибини
	for (int cascade = 0; cascade < SHADOW_CASCADE_COUNT; cascade++)
	{
		D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
		depthViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		depthViewDesc.Texture2DArray.MipSlice = 0;
		depthViewDesc.Texture2DArray.FirstArraySlice = cascade;
		depthViewDesc.Texture2DArray.ArraySize = 1;

		hr = device->CreateDepthStencilView(mDepthTexture, &depthViewDesc, &mDepthStencilViews[cascade]);

		if (FAILED(hr))
			return false;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc = {};
	resourceViewDesc.Format = DXGI_FORMAT_R32_FLOAT;
	resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	resourceViewDesc.Texture2DArray.MostDetailedMip = 0;
	resourceViewDesc.Texture2DArray.MipLevels = 1;
	resourceViewDesc.Texture2DArray.FirstArraySlice = 0;
	resourceViewDesc.Texture2DArray.ArraySize = SHADOW_CASCADE_COUNT;

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

	for (int cascade = 0; cascade < SHADOW_CASCADE_COUNT; cascade++)
	{
		if (mDepthStencilViews[cascade]) mDepthStencilViews[cascade]->Release();
	}

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
	if (!isEnabled())
	{
		constantData->shadowParams[3] = 0.0f;
		return;
	}

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

	// Ближню площину камери беремо з матриці проекції, щоб межі каскадів збігалися з її глибиною
	const Matrix& projection = constantData->projection;

	float nearPlane = 0.1f;

	if (fabsf(projection.mat[2][2]) > 0.0001f)
	{
		nearPlane = -projection.mat[3][2] / projection.mat[2][2];
	}

	if (nearPlane < 0.01f) nearPlane = 0.01f;

	updateCascadeSplits(nearPlane);

	for (int cascade = 0; cascade < SHADOW_CASCADE_COUNT; cascade++)
	{
		updateCascadeMatrix(cascade, direction, mSplits[cascade], mSplits[cascade + 1]);

		constantData->cascadeSplits[cascade] = mSplits[cascade + 1];
	}

	constantData->shadowParams[0] = 1.0f / (float)mResolution;
	constantData->shadowParams[1] = mDepthBias;
	constantData->shadowParams[2] = mTexelBias;
	constantData->shadowParams[3] = mStrength;

	constantData->cascadeParams[1] = mBlendBand;
}

// Обчислює межі каскадів уздовж осі камери
void ShadowMap::updateCascadeSplits(float nearPlane)
{
	mSplits[0] = nearPlane;
	mSplits[SHADOW_CASCADE_COUNT] = mShadowDistance;

	// Практична схема розподілу: логарифмічна дає деталі зблизька, рівномірна - однакові кроки вдалині
	for (int cascade = 1; cascade < SHADOW_CASCADE_COUNT; cascade++)
	{
		float part = (float)cascade / (float)SHADOW_CASCADE_COUNT;

		float logarithmic = nearPlane * powf(mShadowDistance / nearPlane, part);
		float uniform = nearPlane + (mShadowDistance - nearPlane) * part;

		mSplits[cascade] = mSplitLambda * logarithmic + (1.0f - mSplitLambda) * uniform;
	}
}

// Обчислює матрицю виду та проекції світла для одного каскаду
void ShadowMap::updateCascadeMatrix(int cascade, const Vector3& direction, float nearDistance, float farDistance)
{
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	const Matrix& view = constantData->view;
	const Matrix& projection = constantData->projection;

	// Матриця виду є оберненою до матриці камери, тому її стовпці дають осі камери у світі
	Vector3 cameraForward(view.mat[0][2], view.mat[1][2], view.mat[2][2]);

	if (cameraForward.length() < 0.0001f) cameraForward = Vector3(0.0f, 0.0f, 1.0f);

	cameraForward = cameraForward.normalized();

	Vector3 cameraPos(constantData->cameraPos[0], constantData->cameraPos[1], constantData->cameraPos[2]);

	// Тангенси половини кута огляду відновлюються з матриці проекції
	float tanHalfY = fabsf(projection.mat[1][1]) > 0.0001f ? 1.0f / projection.mat[1][1] : 1.0f;
	float tanHalfX = fabsf(projection.mat[0][0]) > 0.0001f ? 1.0f / projection.mat[0][0] : 1.0f;

	float spread = tanHalfX * tanHalfX + tanHalfY * tanHalfY;

	// Каскад обмежується сферою, бо її розмір не залежить від повороту камери і тіні не тремтять
	float centerDistance = (farDistance + nearDistance) * (spread + 1.0f) * 0.5f;
	float offset = farDistance - centerDistance;
	float radius = sqrtf(farDistance * farDistance * spread + offset * offset);

	if (radius < 0.0001f) radius = 0.0001f;

	Vector3 center = cameraPos + cameraForward * centerDistance;

	// Будує ортонормований базис світла, обираючи опорний вектор так, щоб він не був колінеарним напрямку
	Vector3 lightDirection = direction;
	Vector3 reference = fabsf(lightDirection.y) > 0.99f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
	Vector3 right = reference.cross(lightDirection).normalized();
	Vector3 up = lightDirection.cross(right);

	// Прив'язує центр до сітки текселів, інакше тіні мерехтять під час руху камери
	float area = radius * 2.0f;
	float texelSize = area / (float)mResolution;

	float centerRight = floorf((center * right) / texelSize) * texelSize;
	float centerUp = floorf((center * up) / texelSize) * texelSize;
	float centerForward = center * lightDirection;

	center = right * centerRight + up * centerUp + lightDirection * centerForward;

	// Відносить світло назад проти свого напрямку, щоб у проекцію потрапили і об'єкти позаду каскаду
	float depthRange = area + mCasterDistance;

	Vector3 lightPosition = center - lightDirection * (radius + mCasterDistance);

	Matrix lightWorld;
	lightWorld.setIdentity();

	lightWorld.mat[0][0] = right.x; lightWorld.mat[0][1] = right.y; lightWorld.mat[0][2] = right.z;
	lightWorld.mat[1][0] = up.x; lightWorld.mat[1][1] = up.y; lightWorld.mat[1][2] = up.z;
	lightWorld.mat[2][0] = lightDirection.x; lightWorld.mat[2][1] = lightDirection.y; lightWorld.mat[2][2] = lightDirection.z;

	lightWorld.setTranslation(lightPosition);

	Matrix lightView = lightWorld;
	lightView.inverse();

	Matrix lightProjection;
	lightProjection.setOrthoPM(area, area, 0.0f, depthRange);

	constantData->lightViewProjection[cascade] = lightView * lightProjection;

	// Чим більші текселі каскаду, тим більший зсув потрібен, щоб поверхня не затінювала саму себе
	constantData->cascadeBias[cascade] = (mDepthBias + mTexelBias * texelSize) / depthRange;
}

// Готує спільний стан конвеєра для проходу карти тіней
void ShadowMap::begin()
{
	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	// Знімає карту тіней з піксельного шейдера, щоб звільнити її для запису
	deviceContext->setShaderResource(nullptr, 1);

	deviceContext->setViewportSize(mResolution, mResolution);
	deviceContext->setRasterizer(mRasterState);

	// Проходу потрібна лише глибина, тому піксельний шейдер не використовується
	deviceContext->setVertexShader(mDepthVertexShader);
	deviceContext->setPixelShader(nullptr);

	deviceContext->setConstantBuffer(mDepthVertexShader, GraphicsEngine::get()->getGlobalResources()->getConstantBuffer(), 0);
}

// Очищає буфер глибини вказаного каскаду та робить його ціллю рендеру
void ShadowMap::beginCascade(int cascade)
{
	if (cascade < 0 || cascade >= SHADOW_CASCADE_COUNT) return;

	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();

	// Шейдер глибини бере матрицю саме того каскаду, який зараз рендериться
	GraphicsEngine::get()->getGlobalResources()->getConstantData()->cascadeParams[0] = (float)cascade;

	deviceContext->clearDepthTarget(mDepthStencilViews[cascade]);
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

// Встановлює відстань, до якої від камери будуються тіні
void ShadowMap::setShadowDistance(float distance)
{
	mShadowDistance = distance;
}

// Встановлює запас глибини, у межах якого об'єкти позаду каскаду ще кидають тінь
void ShadowMap::setCasterDistance(float distance)
{
	mCasterDistance = distance;
}

// Встановлює розподіл меж каскадів: 0 - рівномірний, 1 - логарифмічний
void ShadowMap::setSplitLambda(float lambda)
{
	if (lambda < 0.0f) lambda = 0.0f;
	if (lambda > 1.0f) lambda = 1.0f;

	mSplitLambda = lambda;
}

// Встановлює зсув глибини у світових одиницях та його множник за розміром текселя
void ShadowMap::setBias(float depthBias, float texelBias)
{
	mDepthBias = depthBias;
	mTexelBias = texelBias;
}

// Встановлює ширину зони, у якій сусідні каскади плавно змішуються
void ShadowMap::setBlendBand(float band)
{
	mBlendBand = band;
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
	return mEnabled && mShaderResourceView != nullptr;
}

// Повертає роздільність одного каскаду
UINT ShadowMap::getResolution()
{
	return mResolution;
}

// Повертає кількість каскадів
int ShadowMap::getCascadeCount()
{
	return SHADOW_CASCADE_COUNT;
}

// Повертає піраміду видимості вказаного каскаду для відсікання об'єктів
Frustum ShadowMap::getCascadeFrustum(int cascade)
{
	if (cascade < 0 || cascade >= SHADOW_CASCADE_COUNT) return Frustum();

	return Frustum(GraphicsEngine::get()->getGlobalResources()->getConstantData()->lightViewProjection[cascade]);
}
