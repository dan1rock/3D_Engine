#pragma once
#include "ResourceManager.h"
#include "ConvexMesh.h"

class ConvexMeshManager : public ResourceManager
{
public:
	ConvexMeshManager();
	~ConvexMeshManager();

	ConvexMesh* createConvexMeshFromFile(const wchar_t* fullPath);

protected:
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath);
};

