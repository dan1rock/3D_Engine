#include "TextureManager.h"
#include <iostream>

TextureManager::TextureManager(): ResourceManager()
{
}

TextureManager::~TextureManager()
{
}

// Створює або повертає текстуру з файлу, використовуючи кешування
Texture* TextureManager::createTextureFromFile(const wchar_t* filePath)
{
	return static_cast<Texture*>(ResourceManager::createResourceFromFile(filePath));
}

// Створює новий об'єкт Texture з файлу (фабричний метод)
Resource* TextureManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	Texture* tex = nullptr;

	try
	{
		tex = new Texture(filePath);
	}
	catch (...) 
	{
		std::wcout << L"Texture creation error: " << filePath << std::endl;
	}

	return tex;
}
