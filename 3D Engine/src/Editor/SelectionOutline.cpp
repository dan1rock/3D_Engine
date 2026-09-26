#include "SelectionOutline.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "ConstantBuffer.h"
#include "GlobalResources.h"
#include "EntityManager.h"
#include "Entity.h"
#include "Renderer.h"
#include <iostream>

// Слот текстури, з якого прохід обведення читає маску
static const UINT MASK_SLOT = 2;

// Дані константного буфера маски; розмір кратний шістнадцяти байтам, як вимагає D3D11
__declspec(align(16))
struct OutlineMaskData
{
	float value[4];
};

// Звільняє маску та стани конвеєра
SelectionOutline::~SelectionOutline()
{
	releaseMask();

	if (mMaxBlendState) mMaxBlendState->Release();
	if (mNoDepthState) mNoDepthState->Release();
	if (mRasterState) mRasterState->Release();
}

// Завантажує шейдери, створює константний буфер і стани конвеєра
bool SelectionOutline::init()
{
	GraphicsEngine* graphics = GraphicsEngine::get();

	// Маска малюється звичайним вершинним шейдером сцени, тож силует точно збігається з об'єктом
	mMaskVertexShader = graphics->getVertexShader(L"src\\Shaders\\VertexShader.hlsl", "main");
	mMaskPixelShader = graphics->getPixelShader(L"src\\Shaders\\OutlineMaskPixelShader.hlsl", "main");
	mFullscreenVertexShader = graphics->getVertexShader(L"src\\Shaders\\PostProcessVertexShader.hlsl", "main");
	mOutlinePixelShader = graphics->getPixelShader(L"src\\Shaders\\OutlinePixelShader.hlsl", "main");

	if (mMaskVertexShader == nullptr || mMaskPixelShader == nullptr
		|| mFullscreenVertexShader == nullptr || mOutlinePixelShader == nullptr)
	{
		std::cout << "Failed to load selection outline shaders" << std::endl;
		return false;
	}

	OutlineMaskData data = {};

	mMaskBuffer = graphics->createConstantBuffer();
	if (!mMaskBuffer->load(&data, sizeof(data))) return false;

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	if (FAILED(graphics->mD3dDevice->CreateBlendState(&blendDesc, &mMaxBlendState))) return false;

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = FALSE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;

	if (FAILED(graphics->mD3dDevice->CreateDepthStencilState(&depthDesc, &mNoDepthState))) return false;

	CD3D11_RASTERIZER_DESC rastDesc(D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE,
		0, D3D11_DEFAULT_DEPTH_BIAS_CLAMP, 0.0f, TRUE, FALSE, FALSE, FALSE);

	if (FAILED(graphics->mD3dDevice->CreateRasterizerState(&rastDesc, &mRasterState))) return false;

	mReady = true;

	return true;
}

// Малює обведення об'єкта та його нащадків поверх кадру у вікно swapChain розміром width x height
void SelectionOutline::render(SwapChain* swapChain, unsigned int width, unsigned int height, Entity* selected)
{
	if (!mReady || selected == nullptr || width == 0 || height == 0) return;

	if (!resizeMask(width, height)) return;

	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();
	ConstantBuffer* constants = GraphicsEngine::get()->getGlobalResources()->getConstantBuffer();

	// Маска минулого кадру ще прив'язана до піксельного шейдера і не може бути ціллю рендеру
	deviceContext->setShaderResource(nullptr, MASK_SLOT);
	deviceContext->clearRenderTarget(mMaskTarget, 0.0f, 0.0f, 0.0f, 0.0f);
	deviceContext->setViewportSize(width, height);

	deviceContext->setRasterizer(mRasterState);
	deviceContext->setBlendState(mMaxBlendState);
	deviceContext->setDepthStencilState(mNoDepthState);

	deviceContext->setVertexShader(mMaskVertexShader);
	deviceContext->setPixelShader(mMaskPixelShader);
	deviceContext->setConstantBuffer(mMaskVertexShader, constants, 0);
	deviceContext->setConstantBuffer(mMaskPixelShader, mMaskBuffer, 0);

	bool drewAnything = false;

	for (Renderer* renderer : EntityManager::get()->getRenderers())
	{
		Entity* owner = renderer->getOwner();

		if (!owner->isActive()) continue;

		// Сам вибраний об'єкт пише в червоний канал, його нащадки на будь-якій глибині - у зелений
		bool isSelected = owner == selected;
		bool isChild = false;

		for (Entity* parent = owner->getParent(); parent && !isSelected; parent = parent->getParent())
		{
			if (parent == selected) { isChild = true; break; }
		}

		if (!isSelected && !isChild) continue;

		OutlineMaskData data = {};
		data.value[0] = isSelected ? 1.0f : 0.0f;
		data.value[1] = isChild ? 1.0f : 0.0f;

		mMaskBuffer->update(deviceContext, &data);

		renderer->renderGeometry();

		drewAnything = true;
	}

	// Повноекранний прохід фарбує краї силуетів прямо у вікно, поверх сцени та сітки
	deviceContext->setRenderTarget(swapChain, false);

	if (drewAnything)
	{
		deviceContext->setBlendState(nullptr);

		deviceContext->setVertexShader(mFullscreenVertexShader);
		deviceContext->setPixelShader(mOutlinePixelShader);
		deviceContext->setShaderResource(mMaskView, MASK_SLOT);
		deviceContext->setFullscreenTriangle();
		deviceContext->drawTriangleList(3, 0);

		deviceContext->setShaderResource(nullptr, MASK_SLOT);
	}

	// Решта рушія розраховує на стандартні стани змішування та глибини
	deviceContext->setBlendState(nullptr);
	deviceContext->setDepthStencilState(nullptr);
}

// Перестворює маску, якщо розмір вікна змінився
bool SelectionOutline::resizeMask(unsigned int width, unsigned int height)
{
	if (mMaskTexture && mMaskWidth == width && mMaskHeight == height) return true;

	releaseMask();

	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	// Двох каналів досить: вибраний об'єкт і його нащадки
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.Format = DXGI_FORMAT_R8G8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;

	if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &mMaskTexture))
		|| FAILED(device->CreateRenderTargetView(mMaskTexture, nullptr, &mMaskTarget))
		|| FAILED(device->CreateShaderResourceView(mMaskTexture, nullptr, &mMaskView)))
	{
		releaseMask();
		return false;
	}

	mMaskWidth = width;
	mMaskHeight = height;

	return true;
}

// Звільняє текстуру маски разом з її переглядами
void SelectionOutline::releaseMask()
{
	if (mMaskView) mMaskView->Release();
	if (mMaskTarget) mMaskTarget->Release();
	if (mMaskTexture) mMaskTexture->Release();

	mMaskView = nullptr;
	mMaskTarget = nullptr;
	mMaskTexture = nullptr;
	mMaskWidth = 0;
	mMaskHeight = 0;
}
