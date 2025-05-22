#include "MeshManager.h"

MeshManager::MeshManager() : ResourceManager()
{
}

MeshManager::~MeshManager()
{
}

// Створює або повертає меш з файлу, використовуючи кешування
Mesh* MeshManager::createMeshFromFile(const wchar_t* filePath)
{
	return static_cast<Mesh*>(ResourceManager::createResourceFromFile(filePath));
}

// Створює новий об'єкт Mesh з файлу (фабричний метод)
Resource* MeshManager::createResourceFromFileConcrete(const wchar_t* filePath)
{
	Mesh* mesh = nullptr;

	try
	{
		mesh = new Mesh(filePath);
	}
	catch (...) {}

	return mesh;
}
