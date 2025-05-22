#pragma once
#include "Resource.h"

class ConvexMesh : public Resource
{
public:
	ConvexMesh(const wchar_t* fullPath);
	~ConvexMesh();

	// Повертає вказівник на опуклий меш, створює його при необхідності
	void* getConvexMesh();
	// Повертає вказівник на трикутний меш, створює його при необхідності
	void* getTriangleMesh();

private:
	void* mConvexMesh = nullptr;
	void* mTriangleMesh = nullptr;

	// Створює опуклий меш з моделі, завантаженої через Assimp
	void createConvexMesh();
	// Створює трикутний меш з моделі, завантаженої через Assimp
	void createTriangleMesh();
};

