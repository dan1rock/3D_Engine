#pragma once
#include "Resource.h"

class ConvexMesh : public Resource
{
public:
	ConvexMesh(const wchar_t* fullPath);
	~ConvexMesh();

	void* getConvexMesh();
	void* getTriangleMesh();

private:
	void* mConvexMesh = nullptr;
	void* mTriangleMesh = nullptr;

	void createConvexMesh();
	void createTriangleMesh();
};

