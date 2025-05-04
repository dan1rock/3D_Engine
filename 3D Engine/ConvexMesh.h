#pragma once
#include "Resource.h"

class ConvexMesh : public Resource
{
public:
	ConvexMesh(const wchar_t* fullPath);
	~ConvexMesh();

	void* getConvexMesh();

private:
	void* mConvexMesh = nullptr;
};

