#include "TextureManager.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "Mesh.h"
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
		// Назва модель::*номер означає текстуру, вбудовану у файл моделі
		std::wstring path(filePath);
		size_t separator = path.find(L"::");

		if (separator == std::wstring::npos)
		{
			tex = new Texture(filePath);
		}
		else
		{
			Mesh* model = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(path.substr(0, separator).c_str());
			const EmbeddedTexture* source = model ? model->findEmbeddedTexture(path.substr(separator + 2)) : nullptr;

			if (source) tex = new Texture(filePath, *source);
		}
	}
	catch (...) 
	{
		std::wcout << L"Texture creation error: " << filePath << std::endl;
	}

	return tex;
}
