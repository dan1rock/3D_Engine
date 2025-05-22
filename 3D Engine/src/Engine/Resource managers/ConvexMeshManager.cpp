#include "ConvexMeshManager.h"

ConvexMeshManager::ConvexMeshManager() : ResourceManager()
{
}

ConvexMeshManager::~ConvexMeshManager()
{
}

// Створює або повертає опуклий меш з файлу, використовуючи кешування
ConvexMesh* ConvexMeshManager::createConvexMeshFromFile(const wchar_t* fullPath)
{
	return static_cast<ConvexMesh*>(ResourceManager::createResourceFromFile(fullPath));
}

// Створює новий об'єкт ConvexMesh з файлу (фабричний метод)
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
