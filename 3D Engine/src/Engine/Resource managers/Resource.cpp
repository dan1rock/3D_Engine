#include "Resource.h"

Resource::Resource(const wchar_t* fullPath) : fullPath(fullPath)
{
}

Resource::~Resource()
{
}

// Повертає повний шлях до ресурсу
std::wstring Resource::getFullPath()
{
	return fullPath;
}
