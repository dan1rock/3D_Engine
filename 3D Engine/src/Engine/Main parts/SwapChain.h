#pragma once
#include <d3d11.h>

class DeviceContext;

class SwapChain
{
public:
	SwapChain();
	bool init(HWND hwnd, UINT width, UINT height);
	bool present(bool vSync);
	bool release();
	~SwapChain();
private:
	IDXGISwapChain* mSwapChain = nullptr;
	ID3D11RenderTargetView* mRenderTargetView = nullptr;
	ID3D11DepthStencilView* mDepthStencilView = nullptr;

	friend class DeviceContext;
};
