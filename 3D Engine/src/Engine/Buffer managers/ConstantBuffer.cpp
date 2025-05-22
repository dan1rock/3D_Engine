#include "ConstantBuffer.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"

ConstantBuffer::ConstantBuffer()
{
}

// Створює та завантажує константний буфер з переданими даними
bool ConstantBuffer::load(void* buffer, UINT bufferSize)
{
	if (mBuffer) mBuffer->Release();

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = bufferSize;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA subresData = {};
	subresData.pSysMem = buffer;

	HRESULT hr;
	hr = GraphicsEngine::get()->mD3dDevice->CreateBuffer(&bufferDesc, &subresData, &mBuffer);
	if (FAILED(hr))
		return false;

	return true;
}

// Оновлює дані в константному буфері для заданого контексту пристрою
void ConstantBuffer::update(DeviceContext* context, void* buffer)
{
	context->mDeviceContext->UpdateSubresource(this->mBuffer, NULL, NULL, buffer, NULL, NULL);
}

// Звільняє ресурси константного буфера
bool ConstantBuffer::release()
{
	mBuffer->Release();
	delete this;
	return true;
}

ConstantBuffer::~ConstantBuffer()
{
}
