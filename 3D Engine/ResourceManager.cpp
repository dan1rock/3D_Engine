#include "ResourceManager.h"
#include <string>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental\filesystem>
#include "DirectXTex.h"

ResourceManager::ResourceManager()
{
}

Texture* ResourceManager::createResourceFromFile(const wchar_t* path)
{
	std::wstring fullPath = std::experimental::filesystem::absolute(path);

	auto it = resourceMap.find(fullPath);

	if (it != resourceMap.end())
		return it->second;

	Texture* rawRes = this->createResourceFromFileConcrete(fullPath.c_str());

	if (rawRes)
	{
		Texture* res(rawRes);
		resourceMap[fullPath] = res;
		return res;
	}

	return nullptr;
}

ResourceManager::~ResourceManager()
{
}

Texture* ResourceManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	return nullptr;
}
