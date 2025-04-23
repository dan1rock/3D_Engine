#pragma once
#include <unordered_map>
#include <string>
#include <filesystem>
#include "Texture.h"

class ResourceManager
{
public:
	ResourceManager();
	Texture* createResourceFromFile(const wchar_t* path);
	~ResourceManager();
	Texture* createResourceFromFileConcrete(const wchar_t* filePath);
private:
	std::unordered_map<std::wstring, Texture*> resourceMap;
};

