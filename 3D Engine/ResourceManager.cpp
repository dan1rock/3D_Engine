#include "ResourceManager.h"
#include <string>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental\filesystem>
#include "DirectXTex.h"

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

	if (it != resourceMap.end())
		return it->second;

	Resource* rawRes = this->createResourceFromFileConcrete(fullPath.c_str());

	if (rawRes)
	{
		resourceMap[fullPath] = rawRes;
		return rawRes;
	}

	return nullptr;
}

Resource* ResourceManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	return nullptr;
}
