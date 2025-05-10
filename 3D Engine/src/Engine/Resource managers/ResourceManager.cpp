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

Resource* ResourceManager::createResourceFromFile(const wchar_t* path)
{
	std::wstring fullPath = std::experimental::filesystem::absolute(path);

	auto it = resourceMap.find(fullPath);

	Resource* rawRes = nullptr;

	if (it != resourceMap.end())
	{
		rawRes = it->second;
		rawRes->isUsed = true;
		std::wcout << L"Resource found in cache: " << fullPath << std::endl;
		return rawRes;
	}

	rawRes = this->createResourceFromFileConcrete(fullPath.c_str());

	if (rawRes)
	{
		resourceMap[fullPath] = rawRes;
		rawRes->isUsed = true;
		std::wcout << L"Resource created and added to cache: " << fullPath << std::endl;
		return rawRes;
	}

	return nullptr;
}

Resource* ResourceManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	return nullptr;
}

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
