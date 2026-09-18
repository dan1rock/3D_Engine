#include "Texture.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include <DirectXTex.h>

// Створює текстуру з файлу
Texture::Texture(const wchar_t* fullPath): Resource(fullPath)
{
	DirectX::ScratchImage imageData;
	HRESULT res = DirectX::LoadFromWICFile(fullPath, DirectX::WIC_FLAGS_NONE, nullptr, imageData);

	if (FAILED(res))
	{
		throw std::exception("Texture creation failed");
	}

	// Без mip-рівнів анізотропна фільтрація не має між чим інтерполювати, тому будуємо їх одразу
	if (createWithMipMaps(imageData)) return;

	res = DirectX::CreateTexture(GraphicsEngine::get()->mD3dDevice, imageData.GetImages(),
		imageData.GetImageCount(), imageData.GetMetadata(), &mTexture);

	if (FAILED(res))
	{
		throw std::exception("Texture creation failed");
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
	desc.Format = imageData.GetMetadata().format;
	desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	desc.Texture2D.MipLevels = (UINT)imageData.GetMetadata().mipLevels;
	desc.Texture2D.MostDetailedMip = 0;

	GraphicsEngine::get()->mD3dDevice->CreateShaderResourceView(mTexture, &desc,
		&mShaderResourceView);
}

// Створює текстуру з повним ланцюжком mip-рівнів, згенерованим відеокартою
bool Texture::createWithMipMaps(const DirectX::ScratchImage& imageData)
{
	ID3D11Device* device = GraphicsEngine::get()->mD3dDevice;

	const DirectX::TexMetadata& metadata = imageData.GetMetadata();

	// Зображення з готовими рівнями або масиви текстур обробляє звичайний шлях завантаження
	if (metadata.mipLevels != 1 || metadata.arraySize != 1 || metadata.depth != 1)
		return false;

	// Не кожен формат підтримує апаратну генерацію mip-рівнів
	UINT formatSupport = 0;

	if (FAILED(device->CheckFormatSupport(metadata.format, &formatSupport)))
		return false;

	if ((formatSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) == 0)
		return false;

	// Нульова кількість рівнів означає повний ланцюжок аж до одного пікселя
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = (UINT)metadata.width;
	texDesc.Height = (UINT)metadata.height;
	texDesc.Format = metadata.format;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
	texDesc.MipLevels = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.ArraySize = 1;
	texDesc.CPUAccessFlags = 0;

	ID3D11Texture2D* texture = nullptr;

	if (FAILED(device->CreateTexture2D(&texDesc, nullptr, &texture)))
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
	viewDesc.Format = metadata.format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipLevels = (UINT)-1;
	viewDesc.Texture2D.MostDetailedMip = 0;

	if (FAILED(device->CreateShaderResourceView(texture, &viewDesc, &mShaderResourceView)))
	{
		texture->Release();
		return false;
	}

	// Завантажуємо вихідне зображення та добудовуємо решту рівнів на відеокарті
	const DirectX::Image* image = imageData.GetImage(0, 0, 0);

	GraphicsEngine::get()->getImmDeviceContext()->updateTexture(texture, image->pixels, (UINT)image->rowPitch);
	GraphicsEngine::get()->getImmDeviceContext()->generateMips(mShaderResourceView);

	mTexture = texture;

	return true;
}

Texture::~Texture()
{
	mTexture->Release();
	mShaderResourceView->Release();
}
