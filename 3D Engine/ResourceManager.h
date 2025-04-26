#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>
#include "Resource.h"

class ResourceManager
{
public:
	ResourceManager();
	virtual ~ResourceManager();

	Resource* createResourceFromFile(const wchar_t* path);

protected:
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath) = 0;

private:
	std::unordered_map<std::wstring, Resource*> resourceMap;
};

