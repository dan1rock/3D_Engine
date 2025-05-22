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

	// Створює або повертає ресурс з файлу, використовуючи кешування
	Resource* createResourceFromFile(const wchar_t* path);

	// Віртуальний метод для створення ресурсу з файлу (реалізується у спадкоємцях)
	void markResourcesAsUnused();
	// Позначає всі ресурси як невикористані
	void unloadUnusedResources();

protected:
	// Видаляє всі невикористані ресурси з пам'яті та кешу
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath) = 0;

private:
	std::unordered_map<std::wstring, Resource*> resourceMap;
};

