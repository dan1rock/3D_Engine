#include "IndexBuffer.h"
#include "GraphicsEngine.h"

IndexBuffer::IndexBuffer()
{
}

// Створює та завантажує індексний буфер за списком індексів
bool IndexBuffer::load(void* indicesList, UINT listSize)
{
	if (mBuffer) mBuffer->Release();

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = 4 * listSize;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA subresData = {};
	subresData.pSysMem = indicesList;

	mListSize = listSize;

	HRESULT hr;
	hr = GraphicsEngine::get()->mD3dDevice->CreateBuffer(&bufferDesc, &subresData, &mBuffer);
	if (FAILED(hr))
		return false;

	return true;
}

// Повертає кількість індексів у буфері
UINT IndexBuffer::getVertexListSize()
{
	return this->mListSize;
}

// Звільняє ресурси індексного буфера
bool IndexBuffer::release()
{
	mBuffer->Release();
	delete this;
	return true;
}

IndexBuffer::~IndexBuffer()
{
}
