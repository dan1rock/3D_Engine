#include "TextureManager.h"
#include <iostream>

TextureManager::TextureManager(): ResourceManager()
{
}

TextureManager::~TextureManager()
{
}

Texture* TextureManager::createTextureFromFile(const wchar_t* filePath)
{
	return static_cast<Texture*>(ResourceManager::createResourceFromFile(filePath));
}

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
