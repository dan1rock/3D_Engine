#pragma once
#include <d3d11.h>
#include "Resource.h"

namespace DirectX
{
	class ScratchImage;
}

struct EmbeddedTexture;

// Стиснення текстури у відеопам'яті: без стиснення, BC1 чи BC3 залежно від прозорості, або якісніше BC7
enum class TextureCompression
{
	None,
	Normal,
	High
};

class Texture : public Resource
{
public:
	// Створює текстуру з файлу; стиснена за налаштуваннями імпорту текстура береться з кешу
	Texture(const wchar_t* fullPath);
	// Створює текстуру, вбудовану у файл моделі; fullPath - назва, під якою її знаходять інші
	Texture(const wchar_t* fullPath, const EmbeddedTexture& source);
	~Texture();

	// Записує вбудовану текстуру у файл: стиснений файл як є, готові пікселі - у PNG
	static bool writeEmbedded(const EmbeddedTexture& source, const std::wstring& path);
	// Повертає розширення, з яким вбудовану текстуру варто записати у файл
	static std::wstring embeddedExtension(const EmbeddedTexture& source);

	// Повертає стиснення з налаштувань імпорту файлу текстури
	static TextureCompression getCompression(const std::wstring& path);
	// Записує стиснення в налаштування імпорту файлу текстури; без стиснення файл налаштувань не потрібен
	static void setCompression(const std::wstring& path, TextureCompression compression);

	// Перезавантажує текстуру з файлу за поточними налаштуваннями імпорту; матеріали лишаються з тією самою текстурою
	void reload();

	// Повертає кількість mip-рівнів текстури
	unsigned int getMipCount() const;
	// Повертає розмір найдетальнішого рівня текстури в пікселях
	void getSize(unsigned int& width, unsigned int& height) const;
	// Повертає формат пікселів текстури у відеопам'яті
	DXGI_FORMAT getFormat() const;
	// Повертає, скільки байтів займають усі mip-рівні текстури у відеопам'яті
	size_t getMemorySize() const;
private:
	// Завантажує файл текстури: стиснену бере з кешу або стискає, решту створює із зображення
	void load(const std::wstring& path);
	// Створює стиснену текстуру з кешу або стискає зображення з готовими mip-рівнями й зберігає його в кеш; false, якщо стиснути не вдалося
	bool loadCompressed(const std::wstring& path, TextureCompression compression);

	// Створює текстуру відеокарти із завантаженого зображення
	void create(const DirectX::ScratchImage& imageData);
	// Створює текстуру з повним ланцюжком mip-рівнів, згенерованим відеокартою
	bool createWithMipMaps(const DirectX::ScratchImage& imageData);

	ID3D11Resource* mTexture = nullptr;
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;

	friend class DeviceContext;
};

