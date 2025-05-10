#include "SwapChain.h"
#include "GraphicsEngine.h"

SwapChain::SwapChain()
{
}

bool SwapChain::init(HWND hwnd, UINT width, UINT height)
{
	if (mSwapChain != nullptr) mSwapChain->Release();
	if (mRenderTargetView != nullptr) mRenderTargetView->Release();
	if (mDepthStencilView != nullptr) mDepthStencilView->Release();

	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	DXGI_SWAP_CHAIN_DESC desc;
	ZeroMemory(&desc, sizeof(DXGI_SWAP_CHAIN_DESC));
	desc.BufferCount = 1;
	desc.BufferDesc.Width = width;
	desc.BufferDesc.Height = height;
	desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.BufferDesc.RefreshRate.Numerator = 60;
	desc.BufferDesc.RefreshRate.Denominator = 1;
	desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.OutputWindow = hwnd;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Windowed = TRUE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	

	HRESULT hr = GraphicsEngine::get()->mDxgiFactory->CreateSwapChain(device, &desc, &mSwapChain);
	if (FAILED(hr))
		return false;

	ID3D11Texture2D* buffer;
	hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buffer);
	if (FAILED(hr))
		return false;

	hr = device->CreateRenderTargetView(buffer, NULL, &mRenderTargetView);
	buffer->Release();
	if (FAILED(hr))
		return false;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.MiscFlags = 0;
	texDesc.ArraySize = 1;
	texDesc.CPUAccessFlags = 0;

	hr = device->CreateTexture2D(&texDesc, nullptr, &buffer);

	if (FAILED(hr))
		return false;

	hr = device->CreateDepthStencilView(buffer, nullptr, &mDepthStencilView);
	buffer->Release();

	if (FAILED(hr))
		return false;

	return true;
}

bool SwapChain::present(bool vSync)
{
	mSwapChain->Present(vSync, NULL);
	return true;
}

bool SwapChain::release()
{
	mSwapChain->Release();
	delete this;
	return true;
}

SwapChain::~SwapChain()
{
}
