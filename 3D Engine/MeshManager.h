#pragma once
#include "ResourceManager.h"
#include "Mesh.h"

class MeshManager : public ResourceManager
{
public:
	MeshManager();
	~MeshManager();

	Mesh* createMeshFromFile(const wchar_t* filePath);
protected:
	virtual Resource* createResourceFromFileConcrete(const wchar_t* filePath);
};

