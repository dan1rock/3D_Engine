#pragma once
#include <d3d11.h>

class DeviceContext;

class ConstantBuffer
{
public:
	ConstantBuffer();
	// Створює та завантажує константний буфер з переданими даними
	bool load(void* buffer, UINT bufferSize);
	// Оновлює дані в константному буфері для заданого контексту пристрою
	void update(DeviceContext* context, void* buffer);
	// Звільняє ресурси константного буфера
	bool release();
	~ConstantBuffer();
private:
	ID3D11Buffer* mBuffer = nullptr;

	friend class DeviceContext;
};
