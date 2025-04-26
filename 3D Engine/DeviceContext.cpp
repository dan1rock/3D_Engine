#include "DeviceContext.h"
#include "GraphicsEngine.h"
#include "SwapChain.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "VertexShader.h"
#include "PixelShader.h"

DeviceContext::DeviceContext(ID3D11DeviceContext* deviceContext) : mDeviceContext(deviceContext)
{
}

void DeviceContext::clearRenderTarget(SwapChain* swapChain, float r, float g, float b, float a)
{
	FLOAT color[] = { r,g,b,a };
	mDeviceContext->ClearRenderTargetView(swapChain->mRenderTargetView, color);
	mDeviceContext->ClearDepthStencilView(swapChain->mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	mDeviceContext->OMSetRenderTargets(1, &swapChain->mRenderTargetView, swapChain->mDepthStencilView);
}

void DeviceContext::setRasterizer(ID3D11RasterizerState* rasterState)
{
	mDeviceContext->RSSetState(rasterState);
}

void DeviceContext::setVertexBuffer(VertexBuffer* vertexBuffer)
{
	UINT stride = vertexBuffer->mVertSize;
	UINT offset = 0;

	mDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer->mBuffer, &stride, &offset);
	mDeviceContext->IASetInputLayout(vertexBuffer->mInputLayout);
}

void DeviceContext::setIndexBuffer(IndexBuffer* indexBuffer)
{
	mDeviceContext->IASetIndexBuffer(indexBuffer->mBuffer, DXGI_FORMAT_R32_UINT, 0);
}

void DeviceContext::drawTriangleList(UINT vertexCount, UINT startIndex)
{
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDeviceContext->Draw(vertexCount, startIndex);
}

void DeviceContext::drawIndexedTriangleList(UINT indexCount, UINT startIndexLocation, UINT startIndex)
{
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mDeviceContext->DrawIndexed(indexCount, startIndexLocation, startIndex);
}

void DeviceContext::drawTriangleStrip(UINT vertexCount, UINT startIndex)
{
	mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mDeviceContext->Draw(vertexCount, startIndex);
}

void DeviceContext::setViewportSize(UINT width, UINT height)
{
	D3D11_VIEWPORT vp = {};
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;

	mDeviceContext->RSSetViewports(1, &vp);
}

void DeviceContext::setVertexShader(VertexShader* vertexShader)
{
	mDeviceContext->VSSetShader(vertexShader->mVertexShader, nullptr, 0);
}

void DeviceContext::setPixelShader(PixelShader* pixelShader)
{
	mDeviceContext->PSSetShader(pixelShader->mPixelShader, nullptr, 0);
}

void DeviceContext::setTexture(VertexShader* vertexShader, Texture* texture)
{
	mDeviceContext->VSSetShaderResources(0, 1, &texture->mShaderResourceView);
}

void DeviceContext::setTexture(PixelShader* pixelShader, Texture* texture)
{
	mDeviceContext->PSSetShaderResources(0, 1, &texture->mShaderResourceView);
}

void DeviceContext::setConstantBuffer(VertexShader* vertexShader, ConstantBuffer* buffer)
{
	mDeviceContext->VSSetConstantBuffers(0, 1, &buffer->mBuffer);
}

void DeviceContext::setConstantBuffer(PixelShader* pixelShader, ConstantBuffer* buffer)
{
	mDeviceContext->PSSetConstantBuffers(0, 1, &buffer->mBuffer);
}

bool DeviceContext::release()
{
	mDeviceContext->Release();
	delete this;
	return true;
}

DeviceContext::~DeviceContext()
{
}
