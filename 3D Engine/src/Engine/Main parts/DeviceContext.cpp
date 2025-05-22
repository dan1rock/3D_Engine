#include "DeviceContext.h"
#include "GraphicsEngine.h"
#include "SwapChain.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "Texture.h"

DeviceContext::DeviceContext(ID3D11DeviceContext* deviceContext) : mDeviceContext(deviceContext)
{
}

// Очищає ціль рендеру та буфер глибини для вказаного SwapChain
void DeviceContext::clearRenderTarget(SwapChain* swapChain, float r, float g, float b, float a)
{
	FLOAT color[] = { r,g,b,a };
	mDeviceContext->ClearRenderTargetView(swapChain->mRenderTargetView, color);
	mDeviceContext->ClearDepthStencilView(swapChain->mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	mDeviceContext->OMSetRenderTargets(1, &swapChain->mRenderTargetView, swapChain->mDepthStencilView);
}

// Встановлює стан семплера для піксельного та вершинного шейдерів
void DeviceContext::setSamplerState(ID3D11SamplerState* samplerState)
{
	mDeviceContext->PSSetSamplers(0, 1, &samplerState);
	mDeviceContext->VSSetSamplers(0, 1, &samplerState);
}

// Встановлює стан растеризатора
void DeviceContext::setRasterizer(ID3D11RasterizerState* rasterState)
{
	mDeviceContext->RSSetState(rasterState);
}

// Встановлює вершинний буфер
void DeviceContext::setVertexBuffer(VertexBuffer* vertexBuffer)
{
	UINT stride = vertexBuffer->mVertSize;
	UINT offset = 0;

	mDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer->mBuffer, &stride, &offset);
	mDeviceContext->IASetInputLayout(vertexBuffer->mInputLayout);
}

// Встановлює індексний буфер
void DeviceContext::setIndexBuffer(IndexBuffer* indexBuffer)
{
	mDeviceContext->IASetIndexBuffer(indexBuffer->mBuffer, DXGI_FORMAT_R32_UINT, 0);
}

// Рендерить трикутники без індексів, по списку вершин
void DeviceContext::drawTriangleList(UINT vertexCount, UINT startIndex)
{
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDeviceContext->Draw(vertexCount, startIndex);
}

// Рендерить трикутники за індексами
void DeviceContext::drawIndexedTriangleList(UINT indexCount, UINT startIndexLocation, UINT startIndex)
{
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDeviceContext->DrawIndexed(indexCount, startIndexLocation, startIndex);
}

// Рендерить трикутники за смугою
void DeviceContext::drawTriangleStrip(UINT vertexCount, UINT startIndex)
{
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mDeviceContext->Draw(vertexCount, startIndex);
}

// Встановлює розмір вихідного зображення
void DeviceContext::setViewportSize(UINT width, UINT height)
{
	D3D11_VIEWPORT vp = {};
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	mDeviceContext->RSSetViewports(1, &vp);
}

// Встановлює вершинний шейдер
void DeviceContext::setVertexShader(VertexShader* vertexShader)
{
	mDeviceContext->VSSetShader(vertexShader->mVertexShader, nullptr, 0);
}

// Встановлює піксельний шейдер
void DeviceContext::setPixelShader(PixelShader* pixelShader)
{
	mDeviceContext->PSSetShader(pixelShader->mPixelShader, nullptr, 0);
}

// Встановлює текстуру для вершинного шейдера
void DeviceContext::setTexture(VertexShader* vertexShader, Texture* texture)
{
	mDeviceContext->VSSetShaderResources(0, 1, &texture->mShaderResourceView);
}

// Встановлює текстуру для піксельного шейдера
void DeviceContext::setTexture(PixelShader* pixelShader, Texture* texture)
{
	mDeviceContext->PSSetShaderResources(0, 1, &texture->mShaderResourceView);
}

// Встановлює константний буфер для вершинного шейдера
void DeviceContext::setConstantBuffer(VertexShader* vertexShader, ConstantBuffer* buffer, UINT slot)
{
	mDeviceContext->VSSetConstantBuffers(slot, 1, &buffer->mBuffer);
}

// Встановлює константний буфер для піксельного шейдера
void DeviceContext::setConstantBuffer(PixelShader* pixelShader, ConstantBuffer* buffer, UINT slot)
{
	mDeviceContext->PSSetConstantBuffers(slot, 1, &buffer->mBuffer);
}

// Звільняє ресурси контексту пристрою
bool DeviceContext::release()
{
	mDeviceContext->Release();
	delete this;
	return true;
}

DeviceContext::~DeviceContext()
{
}
