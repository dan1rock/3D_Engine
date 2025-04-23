#pragma once
#include <d3d11.h>

class DeviceContext;

class VertexBuffer
{
public:
	VertexBuffer();
	bool load(void* vertList, UINT vertSize, UINT listSize, void* shaderByteCode, SIZE_T byteShaderSize);
	UINT getVertexListSize();
	bool release();
	~VertexBuffer();
private:
	UINT mVertSize = 0;
	UINT mListSize = 0;

	ID3D11Buffer* mBuffer = nullptr;
	ID3D11InputLayout* mInputLayout = nullptr;

	friend class DeviceContext;
};
