#pragma once
#include <d3d11.h>

class SwapChain;
class VertexBuffer;
class IndexBuffer;
class ConstantBuffer;
class VertexShader;
class GraphicsEngine;
class PixelShader;
class Texture;

class DeviceContext
{
public:
	DeviceContext(ID3D11DeviceContext* deviceContext);

	// Очищає ціль рендеру та буфер глибини для вказаного SwapChain
	void clearRenderTarget(SwapChain* swapChain, float r, float g, float b, float a);

	// Встановлює стан семплера для піксельного та вершинного шейдерів
	void setSamplerState(ID3D11SamplerState* samplerState);
	// Встановлює стан растеризатора
	void setRasterizer(ID3D11RasterizerState* rasterState);
	// Встановлює вершинний буфер
	void setVertexBuffer(VertexBuffer* vertexBuffer);
	// Встановлює індексний буфер
	void setIndexBuffer(IndexBuffer* indexBuffer);

	// Рендерить трикутники без індексів, по списку вершин
	void drawTriangleList(UINT vertexCount, UINT startIndex);
	// Рендерить трикутники за індексами
	void drawIndexedTriangleList(UINT indexCount, UINT startIndexLocation, UINT startIndex);
	// Рендерить трикутники за смугою
	void drawTriangleStrip(UINT vertexCount, UINT startIndex);

	// Встановлює розмір вихідного зображення
	void setViewportSize(UINT width, UINT height);

	// Встановлює вершинний шейдер
	void setVertexShader(VertexShader* vertexShader);
	// Встановлює піксельний шейдер
	void setPixelShader(PixelShader* pixelShader);

	// Встановлює текстуру для вершинного шейдера
	void setTexture(VertexShader* vertexShader, Texture* texture);
	// Встановлює текстуру для піксельного шейдера
	void setTexture(PixelShader* pixelShader, Texture* texture);

	// Встановлює константний буфер для вершинного шейдера
	void setConstantBuffer(VertexShader* vertexShader, ConstantBuffer* buffer, UINT slot = 0);
	// Встановлює константний буфер для піксельного шейдера
	void setConstantBuffer(PixelShader* pixelShader, ConstantBuffer* buffer, UINT slot = 0);

	// Звільняє ресурси контексту пристрою
	bool release();
	~DeviceContext();
private:
	ID3D11DeviceContext* mDeviceContext;
	ID3D11RasterizerState* mRasterizer = nullptr;

	friend class ConstantBuffer;
};
