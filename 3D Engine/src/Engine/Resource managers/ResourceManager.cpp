#include "ResourceManager.h"
#include <string>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental\filesystem>
#include "DirectXTex.h"

#include <iostream>

ResourceManager::ResourceManager()
{
}

ResourceManager::~ResourceManager()
{
}

// Створює або повертає ресурс з файлу, використовуючи кешування
Resource* ResourceManager::createResourceFromFile(const wchar_t* path)
{
	std::wstring fullPath = std::experimental::filesystem::absolute(path);

	// Перевіряємо, чи ресурс вже є в кеші
	auto it = resourceMap.find(fullPath);

	Resource* rawRes = nullptr;

	// Якщо ресурс знайдено в кеші, повертаємо його
	if (it != resourceMap.end())
	{
		rawRes = it->second;
		rawRes->isUsed = true;
		std::wcout << L"Resource found in cache: " << fullPath << std::endl;
		return rawRes;
	}

	// Якщо ресурс не знайдено в кеші, створюємо його з файлу
	rawRes = this->createResourceFromFileConcrete(fullPath.c_str());

	// Якщо ресурс успішно створено, додаємо його до кешу
	if (rawRes)
	{
		resourceMap[fullPath] = rawRes;
		rawRes->isUsed = true;
		std::wcout << L"Resource created and added to cache: " << fullPath << std::endl;
		return rawRes;
	}

	return nullptr;
}

// Віртуальний метод для створення ресурсу з файлу (реалізується у спадкоємцях)
Resource* ResourceManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	return nullptr;
}

// Позначає всі ресурси як невикористані
void ResourceManager::markResourcesAsUnused()
{
	for (auto& it : resourceMap)
	{
		Resource* res = it.second;
		if (res)
		{
			res->isUsed = false;
		}
	}
}

// Видаляє всі невикористані ресурси з пам'яті та кешу
void ResourceManager::unloadUnusedResources()
{
	int count = 0;

	for (auto it = resourceMap.begin(); it != resourceMap.end();)
	{
		Resource* res = it->second;
		if (res && !res->isUsed)
		{
			count++;
			delete res;
			it = resourceMap.erase(it);
		}
		else
		{
			++it;
		}
	}

	std::wcout << L"Unloaded " << count << L" resources." << std::endl;
}
