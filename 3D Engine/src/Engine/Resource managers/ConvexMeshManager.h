#pragma once
#include "ResourceManager.h"
#include "ConvexMesh.h"

class ConvexMeshManager : public ResourceManager
{
public:
	ConvexMeshManager();
	~ConvexMeshManager();

	// Створює або повертає опуклий меш з файлу, використовуючи кешування
	ConvexMesh* createConvexMeshFromFile(const wchar_t* fullPath);

protected:
	// Створює новий об'єкт ConvexMesh з файлу (фабричний метод)
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath);
};

