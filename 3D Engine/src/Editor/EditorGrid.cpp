#include "EditorGrid.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "GlobalResources.h"
#include "SwapChain.h"
#include <iostream>

// Звільняє створені стани конвеєра
EditorGrid::~EditorGrid()
{
	if (mBlendState) mBlendState->Release();
	if (mDepthState) mDepthState->Release();
	if (mRasterState) mRasterState->Release();
}

// Завантажує шейдери та створює стани змішування, глибини й растеризатора
bool EditorGrid::init()
{
	GraphicsEngine* graphics = GraphicsEngine::get();

	mVertexShader = graphics->getVertexShader(L"src\\Shaders\\GridVertexShader.hlsl", "main");
	mPixelShader = graphics->getPixelShader(L"src\\Shaders\\GridPixelShader.hlsl", "main");

	if (mVertexShader == nullptr || mPixelShader == nullptr)
	{
		std::cout << "Failed to load editor grid shaders" << std::endl;
		return false;
	}

	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	if (FAILED(graphics->mD3dDevice->CreateBlendState(&blendDesc, &mBlendState))) return false;

	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	if (FAILED(graphics->mD3dDevice->CreateDepthStencilState(&depthDesc, &mDepthState))) return false;

	CD3D11_RASTERIZER_DESC rastDesc(D3D11_FILL_SOLID, D3D11_CULL_NONE, FALSE,
		0, D3D11_DEFAULT_DEPTH_BIAS_CLAMP, 0.0f, TRUE, FALSE, FALSE, FALSE);

	if (FAILED(graphics->mD3dDevice->CreateRasterizerState(&rastDesc, &mRasterState))) return false;

	mReady = true;

	return true;
}

// Малює сітку поверх кадру у вікно swapChain, використовуючи глибину відрендереної сцени
void EditorGrid::render(SwapChain* swapChain)
{
	if (!mReady) return;

	DeviceContext* deviceContext = GraphicsEngine::get()->getImmDeviceContext();
	ConstantBuffer* constants = GraphicsEngine::get()->getGlobalResources()->getConstantBuffer();

	// Постобробка вже вивела кадр у вікно, а буфер глибини сцени лишився незмінним, тож сітка
	// малюється після неї і не потрапляє під світіння та віньєтку
	deviceContext->setRenderTarget(swapChain, true);

	deviceContext->setRasterizer(mRasterState);
	deviceContext->setBlendState(mBlendState);
	deviceContext->setDepthStencilState(mDepthState);

	deviceContext->setVertexShader(mVertexShader);
	deviceContext->setPixelShader(mPixelShader);
	deviceContext->setConstantBuffer(mVertexShader, constants, 0);
	deviceContext->setConstantBuffer(mPixelShader, constants, 0);

	deviceContext->setFullscreenTriangle();
	deviceContext->drawTriangleList(6, 0);

	// Решта рушія розраховує на стандартні стани змішування та глибини
	deviceContext->setBlendState(nullptr);
	deviceContext->setDepthStencilState(nullptr);
}
