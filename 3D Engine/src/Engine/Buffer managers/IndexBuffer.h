#pragma once
#include <d3d11.h>

class DeviceContext;

class IndexBuffer
{
public:
	IndexBuffer();
	// Створює та завантажує індексний буфер за списком індексів
	bool load(void* indicesList, UINT listSize);
	// Повертає кількість індексів у буфері
	UINT getVertexListSize();
	// Звільняє ресурси індексного буфера
	bool release();
	~IndexBuffer();
private:
	UINT mListSize = 0;

	ID3D11Buffer* mBuffer = nullptr;

	friend class DeviceContext;
};
