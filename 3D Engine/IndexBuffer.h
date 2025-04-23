#pragma once
#include <d3d11.h>

class DeviceContext;

class IndexBuffer
{
public:
	IndexBuffer();
	bool load(void* indicesList, UINT listSize);
	UINT getVertexListSize();
	bool release();
	~IndexBuffer();
private:
	UINT mListSize = 0;

	ID3D11Buffer* mBuffer = nullptr;

	friend class DeviceContext;
};
