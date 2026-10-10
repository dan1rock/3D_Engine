#pragma once
#include "Resource.h"
#include <string>

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

	// Створює опуклий меш з кешу готових даних або з геометрії вказаного меша чи моделі з того самого файлу
	void createConvexMesh(Mesh* sourceMesh);
	// Створює трикутний меш з кешу готових даних або з геометрії вказаного меша чи моделі з того самого файлу
	void createTriangleMesh(Mesh* sourceMesh);
	// Повертає відбиток, за яким готові фізичні дані в кеші відповідають файлу моделі та версії PhysX
	std::string cacheStamp();
	// Повертає меш з геометрією для готування: вказаний, якщо він має вершини, або завантажений з того самого файлу
	Mesh* geometrySource(Mesh* sourceMesh);
};

