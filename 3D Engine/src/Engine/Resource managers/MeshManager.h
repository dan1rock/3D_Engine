#pragma once
#include "ResourceManager.h"
#include "Mesh.h"

class MeshManager : public ResourceManager
{
public:
	MeshManager();
	~MeshManager();

	// Створює або повертає опуклий меш з файлу, використовуючи кешування
	Mesh* createMeshFromFile(const wchar_t* filePath);
protected:
	// Створює новий об'єкт Mesh з файлу (фабричний метод)
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath);
};

