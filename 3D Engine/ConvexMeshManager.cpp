#include "ConvexMeshManager.h"

ConvexMeshManager::ConvexMeshManager() : ResourceManager()
{
}

ConvexMeshManager::~ConvexMeshManager()
{
}

ConvexMesh* ConvexMeshManager::createConvexMeshFromFile(const wchar_t* fullPath)
{
	return static_cast<ConvexMesh*>(ResourceManager::createResourceFromFile(fullPath));
}

Resource* ConvexMeshManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	ConvexMesh* mesh = nullptr;

	try
	{
		mesh = new ConvexMesh(filePath);
	}
	catch (...) {}

	return mesh;
}
