#pragma once
#include <d3d11.h>
#include "Resource.h"

namespace DirectX
{
	class ScratchImage;
}

class Texture : public Resource
{
public:
	// Створює текстуру з файлу
	Texture(const wchar_t* fullPath);
	~Texture();
private:
	// Створює текстуру з повним ланцюжком mip-рівнів, згенерованим відеокартою
	bool createWithMipMaps(const DirectX::ScratchImage& imageData);

	ID3D11Resource* mTexture = nullptr;
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;

	friend class DeviceContext;
};

