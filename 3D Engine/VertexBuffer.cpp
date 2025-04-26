#include "VertexBuffer.h"
#include "GraphicsEngine.h"

VertexBuffer::VertexBuffer()
{
}

bool VertexBuffer::load(void* vertList, UINT vertSize, UINT listSize, void* shaderByteCode, SIZE_T byteShaderSize)
{
	if (mBuffer) mBuffer->Release();
	if (mInputLayout) mInputLayout->Release();

	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = vertSize * listSize;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA subresData = {};
	subresData.pSysMem = vertList;

	mVertSize = vertSize;
	mListSize = listSize;

	HRESULT hr;
	hr = GraphicsEngine::get()->mD3dDevice->CreateBuffer(&bufferDesc, &subresData, &mBuffer);
	if (FAILED(hr))
		return false;

	D3D11_INPUT_ELEMENT_DESC inputElementDesc[] = {
		//SEMANTIC NAME - SEMANTIC INDEX - FORMAT - INPUT SLOT - ALIGNED BYTE OFFSET - INPUT SLOT CLASS - INSTANCE DATA STEP RATE
		{"POSITION"	, 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD"	, 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	UINT inputElementDescSize = ARRAYSIZE(inputElementDesc);

	hr = GraphicsEngine::get()->mD3dDevice->CreateInputLayout(inputElementDesc, inputElementDescSize, shaderByteCode, byteShaderSize, &mInputLayout);
	if (FAILED(hr))
		return false;

	return true;
}

UINT VertexBuffer::getVertexListSize()
{
	return this->mListSize;
}

bool VertexBuffer::release()
{
	mBuffer->Release();
	mInputLayout->Release();
	delete this;
	return true;
}

VertexBuffer::~VertexBuffer()
{
}
