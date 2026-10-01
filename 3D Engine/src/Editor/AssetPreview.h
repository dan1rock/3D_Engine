#pragma once
#include <d3d11.h>
#include <string>
#include <vector>
#include <map>

class PixelShader;
class VertexShader;

// Мініатюри ресурсів для панелі ресурсів: моделі, префаби й матеріали малюються в невеликі зображення, текстури копіюються в них
class AssetPreview
{
public:
	// Вид ресурсу, від якого залежить, як малюється його мініатюра
	enum class Kind { Model, Prefab, Material, Texture };

	// Розмір мініатюри в пікселях
	static const int SIZE = 128;

	// Створює буфер глибини, шейдер копіювання текстур і семплер
	bool init();
	// Звільняє всі мініатюри та ресурси відеокарти
	void release();

	// Повертає мініатюру ресурсу або nullptr, поки її ще не намальовано; застарілу мініатюру ставить у чергу на перемальовування
	ID3D11ShaderResourceView* get(Kind kind, const std::string& path);
	// Малює одну мініатюру з черги; викликається раз на кадр до рендеру сцени
	void renderPending();

private:
	// Мініатюра одного ресурсу: зображення на відеокарті, стан файлу, з якого її намальовано, і файл кешу
	struct Thumbnail
	{
		ID3D11Resource* resource = nullptr;
		ID3D11ShaderResourceView* view = nullptr;
		std::string stamp;
		std::string file;
		bool queued = false;
		int checkedFrame = -1000;
	};

	// Повертає позначку стану ресурсу: час зміни його файлів, тож змінений ресурс отримує нову мініатюру
	std::string stampOf(Kind kind, const std::string& path) const;
	// Повертає початок шляху файлів кешу мініатюри ресурсу, спільний для всіх його станів
	std::string cachePrefix(const std::string& key) const;
	// Повертає шлях файлу кешу мініатюри для ключа та позначки стану
	std::string cacheFile(const std::string& key, const std::string& stamp) const;
	// Читає мініатюру з файлу кешу; повертає false, якщо файлу немає
	bool loadCached(const std::string& file, Thumbnail& thumbnail);
	// Малює мініатюру ресурсу в нове зображення; повертає false, якщо ресурс не вдалося прочитати
	bool render(Kind kind, const std::string& path, ID3D11Texture2D*& texture);
	// Записує намальовану мініатюру у файл кешу
	void save(ID3D11Texture2D* texture, const std::string& file);
	// Замінює зображення мініатюри новим, звільняючи старе
	void replace(Thumbnail& thumbnail, ID3D11Resource* resource, ID3D11ShaderResourceView* view);

	std::map<std::string, Thumbnail> mThumbnails;
	// Ресурси, чиї мініатюри чекають на малювання: вид і шлях
	std::vector<std::pair<Kind, std::string>> mQueue;

	ID3D11Texture2D* mDepth = nullptr;
	ID3D11DepthStencilView* mDepthView = nullptr;
	ID3D11SamplerState* mSampler = nullptr;
	VertexShader* mFullscreenShader = nullptr;
	PixelShader* mBlitShader = nullptr;
};
