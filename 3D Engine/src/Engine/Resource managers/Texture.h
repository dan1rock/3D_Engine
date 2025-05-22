#pragma once
#include <d3d11.h>
#include "Resource.h"

class Texture : public Resource
{
public:
	// Створює текстуру з файлу
	Texture(const wchar_t* fullPath);
	~Texture();
private:
	ID3D11Resource* mTexture = nullptr;
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;

	friend class DeviceContext;
};

