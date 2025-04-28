#include "MeshManager.h"

MeshManager::MeshManager() : ResourceManager()
{
}

MeshManager::~MeshManager()
{
}

Mesh* MeshManager::createMeshFromFile(const wchar_t* filePath)
{
	return static_cast<Mesh*>(ResourceManager::createResourceFromFile(filePath));
}

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
