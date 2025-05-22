#pragma once
#include "ResourceManager.h"
#include "Texture.h"

class TextureManager : public ResourceManager
{
public:
	TextureManager();
	~TextureManager();

	// Створює або повертає текстуру з файлу, використовуючи кешування
	Texture* createTextureFromFile(const wchar_t* filePath);
protected:
	// Створює новий об'єкт Texture з файлу (фабричний метод)
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath);
};

