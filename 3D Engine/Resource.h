#pragma once
#include <string>

class Resource
{
public:
	Resource(const wchar_t* fullPath);
	virtual ~Resource();

private:
	std::wstring fullPath;
};
