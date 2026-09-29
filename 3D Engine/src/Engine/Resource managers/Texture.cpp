#include "Texture.h"
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include <DirectXTex.h>
#include <fstream>

// Створює текстуру з файлу
Texture::Texture(const wchar_t* fullPath): Resource(fullPath)
{
	DirectX::ScratchImage imageData;
	HRESULT res = DirectX::LoadFromWICFile(fullPath, DirectX::WIC_FLAGS_NONE, nullptr, imageData);

	if (FAILED(res))
	{
		throw std::exception("Texture creation failed");
	}

	create(imageData);
}

// Створює текстуру, вбудовану у файл моделі; fullPath - назва, під якою її знаходять інші
Texture::Texture(const wchar_t* fullPath, const EmbeddedTexture& source): Resource(fullPath)
{
	DirectX::ScratchImage imageData;
	HRESULT res = E_FAIL;

	if (source.height > 0)
	{
		// Готові пікселі лежать у порядку синій, зелений, червоний, прозорість
		res = imageData.Initialize2D(DXGI_FORMAT_B8G8R8A8_UNORM, source.width, source.height, 1, 1);

		if (SUCCEEDED(res))
		{
			const DirectX::Image* image = imageData.GetImage(0, 0, 0);

			for (unsigned int y = 0; y < source.height; y++)
			{
				memcpy(image->pixels + y * image->rowPitch, source.data.data() + (size_t)y * source.width * 4, (size_t)source.width * 4);
			}
		}
	}
	else if (source.format == "dds")
	{
		res = DirectX::LoadFromDDSMemory(source.data.data(), source.data.size(), DirectX::DDS_FLAGS_NONE, nullptr, imageData);
	}
	else
	{
		// Формат стисненого файлу підказує модель; невідомий пробуємо як звичайне зображення, а потім як TGA
		if (source.format != "tga") res = DirectX::LoadFromWICMemory(source.data.data(), source.data.size(), DirectX::WIC_FLAGS_NONE, nullptr, imageData);
		if (FAILED(res)) res = DirectX::LoadFromTGAMemory(source.data.data(), source.data.size(), DirectX::TGA_FLAGS_NONE, nullptr, imageData);
	}

	if (FAILED(res))
	{
		throw std::exception("Embedded texture creation failed");
	}

	create(imageData);
}

// Повертає розширення, з яким вбудовану текстуру варто записати у файл
std::wstring Texture::embeddedExtension(const EmbeddedTexture& source)
{
	if (source.height > 0 || source.format.empty()) return L".png";

	std::wstring format(source.format.begin(), source.format.end());

	return L"." + format;
}

// Записує вбудовану текстуру у файл: стиснений файл як є, готові пікселі - у PNG
bool Texture::writeEmbedded(const EmbeddedTexture& source, const std::wstring& path)
{
	if (source.height == 0)
	{
		std::ofstream file(path, std::ios::binary);

		if (!file.is_open()) return false;

		file.write((const char*)source.data.data(), (std::streamsize)source.data.size());

		return true;
	}

	DirectX::Image image = {};
	image.width = source.width;
	image.height = source.height;
	image.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	image.rowPitch = (size_t)source.width * 4;
	image.slicePitch = image.rowPitch * source.height;
	image.pixels = const_cast<uint8_t*>(source.data.data());

	return SUCCEEDED(DirectX::SaveToWICFile(image, DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str()));
}

// Створює текстуру відеокарти із завантаженого зображення
void Texture::create(const DirectX::ScratchImage& imageData)
{
	// Без mip-рівнів анізотропна фільтрація не має між чим інтерполювати, тому будуємо їх одразу
	if (createWithMipMaps(imageData)) return;

	HRESULT res = DirectX::CreateTexture(GraphicsEngine::get()->mD3dDevice, imageData.GetImages(),
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

// Повертає кількість mip-рівнів текстури
unsigned int Texture::getMipCount() const
{
	ID3D11Texture2D* texture = nullptr;

	if (FAILED(mTexture->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture))) return 1;

	D3D11_TEXTURE2D_DESC desc = {};
	texture->GetDesc(&desc);
	texture->Release();

	return desc.MipLevels;
}

Texture::~Texture()
{
	mTexture->Release();
	mShaderResourceView->Release();
}
