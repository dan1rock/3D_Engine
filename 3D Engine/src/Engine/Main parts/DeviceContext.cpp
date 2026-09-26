#include "DeviceContext.h"
#include "GraphicsEngine.h"
#include "SwapChain.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"
#include "Texture.h"
#include "imgui_impl_dx11.h"

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

// Очищає вказану ціль рендеру разом з буфером глибини SwapChain та робить їх активними
void DeviceContext::clearRenderTarget(ID3D11RenderTargetView* renderTargetView, SwapChain* swapChain, float r, float g, float b, float a)
{
	FLOAT color[] = { r,g,b,a };
	mDeviceContext->ClearRenderTargetView(renderTargetView, color);
	mDeviceContext->ClearDepthStencilView(swapChain->mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	mDeviceContext->OMSetRenderTargets(1, &renderTargetView, swapChain->mDepthStencilView);
}

// Очищає вказану ціль рендеру та робить її єдиною активною, без буфера глибини
void DeviceContext::clearRenderTarget(ID3D11RenderTargetView* renderTargetView, float r, float g, float b, float a)
{
	FLOAT color[] = { r,g,b,a };
	mDeviceContext->ClearRenderTargetView(renderTargetView, color);
	mDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
}

// Повертає ціль рендеру вказаного SwapChain без очищення його буферів
void DeviceContext::setRenderTarget(SwapChain* swapChain, bool withDepthBuffer)
{
	mDeviceContext->OMSetRenderTargets(1, &swapChain->mRenderTargetView,
		withDepthBuffer ? swapChain->mDepthStencilView : nullptr);
}

// Встановлює вказану ціль рендеру без буфера глибини
void DeviceContext::setRenderTarget(ID3D11RenderTargetView* renderTargetView)
{
	mDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
}

// Очищає вказаний буфер глибини та встановлює його єдиною ціллю рендеру
void DeviceContext::clearDepthTarget(ID3D11DepthStencilView* depthStencilView)
{
	mDeviceContext->ClearDepthStencilView(depthStencilView, D3D11_CLEAR_DEPTH, 1, 0);

	ID3D11RenderTargetView* nullRenderTarget = nullptr;
	mDeviceContext->OMSetRenderTargets(1, &nullRenderTarget, depthStencilView);
}

// Знімає всі цілі рендеру, щоб їхні буфери можна було читати в шейдерах
void DeviceContext::unsetRenderTargets()
{
	ID3D11RenderTargetView* nullRenderTarget = nullptr;
	mDeviceContext->OMSetRenderTargets(1, &nullRenderTarget, nullptr);
}

// Встановлює стан семплера для піксельного та вершинного шейдерів у вказаний слот
void DeviceContext::setSamplerState(ID3D11SamplerState* samplerState, UINT slot)
{
	mDeviceContext->PSSetSamplers(slot, 1, &samplerState);
	mDeviceContext->VSSetSamplers(slot, 1, &samplerState);
}

// Встановлює стан растеризатора
void DeviceContext::setRasterizer(ID3D11RasterizerState* rasterState)
{
	mDeviceContext->RSSetState(rasterState);
}

// Встановлює стан змішування кольорів (nullptr - стандартний, без змішування)
void DeviceContext::setBlendState(ID3D11BlendState* blendState)
{
	mDeviceContext->OMSetBlendState(blendState, nullptr, 0xffffffff);
}

// Встановлює стан перевірки глибини (nullptr - стандартний, з перевіркою та записом)
void DeviceContext::setDepthStencilState(ID3D11DepthStencilState* depthStencilState)
{
	mDeviceContext->OMSetDepthStencilState(depthStencilState, 0);
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

// Готує конвеєр до рендеру повноекранного трикутника, який будується без вершинного буфера
void DeviceContext::setFullscreenTriangle()
{
	ID3D11Buffer* nullBuffer = nullptr;
	UINT stride = 0;
	UINT offset = 0;

	mDeviceContext->IASetVertexBuffers(0, 1, &nullBuffer, &stride, &offset);
	mDeviceContext->IASetInputLayout(nullptr);
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
	mDeviceContext->VSSetShader(vertexShader ? vertexShader->mVertexShader : nullptr, nullptr, 0);
}

// Встановлює піксельний шейдер
void DeviceContext::setPixelShader(PixelShader* pixelShader)
{
	mDeviceContext->PSSetShader(pixelShader ? pixelShader->mPixelShader : nullptr, nullptr, 0);
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

// Встановлює ресурс шейдера для піксельного шейдера у вказаний слот
void DeviceContext::setShaderResource(ID3D11ShaderResourceView* shaderResourceView, UINT slot)
{
	mDeviceContext->PSSetShaderResources(slot, 1, &shaderResourceView);
}

// Завантажує зображення в найдетальніший рівень текстури
void DeviceContext::updateTexture(ID3D11Resource* texture, const void* data, UINT rowPitch)
{
	mDeviceContext->UpdateSubresource(texture, 0, nullptr, data, rowPitch, 0);
}

// Будує ланцюжок mip-рівнів текстури засобами відеокарти
void DeviceContext::generateMips(ID3D11ShaderResourceView* shaderResourceView)
{
	mDeviceContext->GenerateMips(shaderResourceView);
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
