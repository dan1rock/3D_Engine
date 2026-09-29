#include "ConvexMesh.h"
#include "PhysicsEngine.h"
#include "GraphicsEngine.h"
#include "MeshManager.h"
#include "Vector3.h"
#include "Vector2.h"
#include "Mesh.h"
#include <vector>
#include <PxPhysics.h>

ConvexMesh::ConvexMesh(const wchar_t* fullPath) : Resource(fullPath)
{
}

ConvexMesh::~ConvexMesh()
{
	if (mConvexMesh)
	{
		static_cast<PxConvexMesh*>(mConvexMesh)->release();
		mConvexMesh = nullptr;
	}
	if (mTriangleMesh)
	{
		static_cast<PxTriangleMesh*>(mTriangleMesh)->release();
		mTriangleMesh = nullptr;
	}
}

// Повертає вказівник на опуклий меш, створює його при необхідності
void* ConvexMesh::getConvexMesh(Mesh* sourceMesh)
{
	if (mConvexMesh == nullptr) createConvexMesh(sourceMesh);
    return mConvexMesh;
}

// Повертає вказівник на трикутний меш, створює його при необхідності
void* ConvexMesh::getTriangleMesh(Mesh* sourceMesh)
{
	if (mTriangleMesh == nullptr) createTriangleMesh(sourceMesh);
	return mTriangleMesh;
}

// Створює опуклий меш з геометрії вказаного меша або з моделі з того самого файлу
void ConvexMesh::createConvexMesh(Mesh* sourceMesh)
{
	std::vector<PxVec3> points;

	// Геометрія вже є в оперативній пам'яті, тому файл читати не потрібно
	if (sourceMesh && !sourceMesh->getPositions().empty())
	{
		const std::vector<Vector3>& positions = sourceMesh->getPositions();

		points.reserve(positions.size());

		for (const Vector3& position : positions) {
			points.emplace_back(PxVec3(position.x, position.y, position.z));
		}

		mConvexMesh = PhysicsEngine::get()->cookConvexMesh(points);
		return;
	}

	// Без готового меша геометрію дає той самий завантажувач моделей, що й для малювання, з перетвореннями вузлів
	Mesh* loaded = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(getFullPath().c_str());

	if (loaded && !loaded->getPositions().empty()) createConvexMesh(loaded);
}

// Створює трикутний меш з геометрії вказаного меша або з моделі з того самого файлу
void ConvexMesh::createTriangleMesh(Mesh* sourceMesh)
{
	std::vector<PxVec3> points;
	std::vector<PxU32> indices;

	// Геометрія вже є в оперативній пам'яті, тому файл читати не потрібно
	if (sourceMesh && !sourceMesh->getPositions().empty())
	{
		const std::vector<Vector3>& positions = sourceMesh->getPositions();
		const std::vector<unsigned int>& meshIndices = sourceMesh->getIndices();

		points.reserve(positions.size());
		indices.reserve(meshIndices.size());

		for (const Vector3& position : positions) {
			points.emplace_back(PxVec3(position.x, position.y, position.z));
		}

		indices.assign(meshIndices.begin(), meshIndices.end());

		mTriangleMesh = PhysicsEngine::get()->cookTriangleMesh(points, indices);

		return;
	}

	// Без готового меша геометрію дає той самий завантажувач моделей, що й для малювання, з перетвореннями вузлів
	Mesh* loaded = GraphicsEngine::get()->getMeshManager()->createMeshFromFile(getFullPath().c_str());

	if (loaded && !loaded->getPositions().empty()) createTriangleMesh(loaded);
}
