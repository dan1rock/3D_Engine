#pragma once
#include <d3d11.h>
#include <string>

class Texture
{
public:
	Texture(const wchar_t* fullPath);
	~Texture();
private:
	ID3D11Resource* mTexture = nullptr;
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;

	std::wstring mFullPath;

	friend class DeviceContext;
};

