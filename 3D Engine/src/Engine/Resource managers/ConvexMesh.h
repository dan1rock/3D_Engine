#pragma once
#include "Resource.h"

class Mesh;

class ConvexMesh : public Resource
{
public:
	ConvexMesh(const wchar_t* fullPath);
	~ConvexMesh();

	// Повертає вказівник на опуклий меш, створює його при необхідності
	// sourceMesh дозволяє взяти геометрію з уже завантаженого меша замість повторного читання файлу
	void* getConvexMesh(Mesh* sourceMesh = nullptr);
	// Повертає вказівник на трикутний меш, створює його при необхідності
	// sourceMesh дозволяє взяти геометрію з уже завантаженого меша замість повторного читання файлу
	void* getTriangleMesh(Mesh* sourceMesh = nullptr);

private:
	void* mConvexMesh = nullptr;
	void* mTriangleMesh = nullptr;

	// Створює опуклий меш з геометрії вказаного меша або з моделі, завантаженої через Assimp
	void createConvexMesh(Mesh* sourceMesh);
	// Створює трикутний меш з геометрії вказаного меша або з моделі, завантаженої через Assimp
	void createTriangleMesh(Mesh* sourceMesh);
};

