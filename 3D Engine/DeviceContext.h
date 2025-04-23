#pragma once
#include <d3d11.h>

class SwapChain;
class VertexBuffer;
class IndexBuffer;
class ConstantBuffer;
class VertexShader;
class GraphicsEngine;
class PixelShader;

class DeviceContext
{
public:
	DeviceContext(ID3D11DeviceContext* deviceContext);
	void clearRenderTarget(SwapChain* swapChain, float r, float g, float b, float a);
	void setRasterizer(ID3D11RasterizerState* rasterState);
	void setVertexBuffer(VertexBuffer* vertexBuffer);
	void setIndexBuffer(IndexBuffer* indexBuffer);
	void drawTriangleList(UINT vertexCount, UINT startIndex);
	void drawIndexedTriangleList(UINT indexCount, UINT startIndexLocation, UINT startIndex);
	void drawTriangleStrip(UINT vertexCount, UINT startIndex);
	void setViewportSize(UINT width, UINT height);
	void setVertexShader(VertexShader* vertexShader);
	void setPixelShader(PixelShader* pixelShader);
	void setConstantBuffer(VertexShader* vertexShader, ConstantBuffer* buffer);
	void setConstantBuffer(PixelShader* pixelShader, ConstantBuffer* buffer);
	bool release();
	~DeviceContext();
private:
	ID3D11DeviceContext* mDeviceContext;
	ID3D11RasterizerState* mRasterizer = nullptr;

	friend class ConstantBuffer;
};
