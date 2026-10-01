#pragma once
#include <d3d11.h>
#include "Resource.h"

namespace DirectX
{
	class ScratchImage;
}

struct EmbeddedTexture;

class Texture : public Resource
{
public:
	// Створює текстуру з файлу
	Texture(const wchar_t* fullPath);
	// Створює текстуру, вбудовану у файл моделі; fullPath - назва, під якою її знаходять інші
	Texture(const wchar_t* fullPath, const EmbeddedTexture& source);
	~Texture();

	// Записує вбудовану текстуру у файл: стиснений файл як є, готові пікселі - у PNG
	static bool writeEmbedded(const EmbeddedTexture& source, const std::wstring& path);
	// Повертає розширення, з яким вбудовану текстуру варто записати у файл
	static std::wstring embeddedExtension(const EmbeddedTexture& source);

	// Повертає кількість mip-рівнів текстури
	unsigned int getMipCount() const;
	// Повертає розмір найдетальнішого рівня текстури в пікселях
	void getSize(unsigned int& width, unsigned int& height) const;
private:
	// Створює текстуру відеокарти із завантаженого зображення
	void create(const DirectX::ScratchImage& imageData);
	// Створює текстуру з повним ланцюжком mip-рівнів, згенерованим відеокартою
	bool createWithMipMaps(const DirectX::ScratchImage& imageData);

	ID3D11Resource* mTexture = nullptr;
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;

	friend class DeviceContext;
};

