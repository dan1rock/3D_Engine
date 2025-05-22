#include "Texture.h"
#include "GraphicsEngine.h"
#include <DirectXTex.h>

// Створює текстуру з файлу
Texture::Texture(const wchar_t* fullPath): Resource(fullPath)
{
	DirectX::ScratchImage imageData;
	HRESULT res = DirectX::LoadFromWICFile(fullPath, DirectX::WIC_FLAGS_NONE, nullptr, imageData);

	if (SUCCEEDED(res))
	{
		res = DirectX::CreateTexture(GraphicsEngine::get()->mD3dDevice, imageData.GetImages(),
			imageData.GetImageCount(), imageData.GetMetadata(), &mTexture);

		D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
		desc.Format = imageData.GetMetadata().format;
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipLevels = (UINT)imageData.GetMetadata().mipLevels;
		desc.Texture2D.MostDetailedMip = 0;

		GraphicsEngine::get()->mD3dDevice->CreateShaderResourceView(mTexture, &desc,
			&mShaderResourceView);
	}
	else
	{
		throw std::exception("Texture creation failed");
	}
}

Texture::~Texture()
{
	mTexture->Release();
	mShaderResourceView->Release();
}
