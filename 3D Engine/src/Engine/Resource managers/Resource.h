#pragma once
#include <string>

// Клас для створення ресурсу
class Resource
{
public:
	Resource(const wchar_t* fullPath);
	virtual ~Resource();

	// Повертає повний шлях до ресурсу
	std::wstring getFullPath();

private:
	std::wstring fullPath;

	bool isUsed = false;

	friend class ResourceManager;
};
