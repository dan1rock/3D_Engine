#pragma once
#include <d3d11.h>

class DeviceContext;

class SwapChain
{
public:
	SwapChain();
	// Ініціалізує SwapChain для заданого вікна з вказаною шириною та висотою
	bool init(HWND hwnd, UINT width, UINT height);
	// Відображає (презентує) зображення на екран, з можливістю вертикальної синхронізації
	bool present(bool vSync);
	// Звільняє ресурси SwapChain
	bool release();
	~SwapChain();
private:
	IDXGISwapChain* mSwapChain = nullptr;
	ID3D11RenderTargetView* mRenderTargetView = nullptr;
	ID3D11DepthStencilView* mDepthStencilView = nullptr;

	friend class DeviceContext;
};
