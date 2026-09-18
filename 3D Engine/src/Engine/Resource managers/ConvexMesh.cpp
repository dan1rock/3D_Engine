#include "ConvexMesh.h"
#include "PhysicsEngine.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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

// Створює опуклий меш з геометрії вказаного меша або з моделі, завантаженої через Assimp
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

	std::wstring ws = getFullPath();
	std::string filePath(ws.begin(), ws.end());

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		filePath,
		aiProcess_Triangulate
		| aiProcess_JoinIdenticalVertices);

	if (!scene || !scene->HasMeshes())
	{
		return;
	}

	for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[m];

		points.reserve(points.size() + mesh->mNumVertices);

		for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
			points.emplace_back(PxVec3(
				mesh->mVertices[i].x,
				mesh->mVertices[i].y,
				mesh->mVertices[i].z));
		}
	}

	mConvexMesh = PhysicsEngine::get()->cookConvexMesh(points);
}

// Створює трикутний меш з геометрії вказаного меша або з моделі, завантаженої через Assimp
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

	std::wstring ws = getFullPath();
	std::string filePath(ws.begin(), ws.end());

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(
		filePath,
		aiProcess_Triangulate);

	if (!scene || !scene->HasMeshes())
	{
		return;
	}

	for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[m];

		// Індекси кожної сітки нумеруються з нуля, тому зміщуємо їх на вже зібрані вершини
		PxU32 baseVertex = static_cast<PxU32>(points.size());

		points.reserve(points.size() + mesh->mNumVertices);
		indices.reserve(indices.size() + mesh->mNumFaces * 3);

		for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
			points.emplace_back(PxVec3(
				mesh->mVertices[i].x,
				mesh->mVertices[i].y,
				mesh->mVertices[i].z));
		}
		for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
			const aiFace& face = mesh->mFaces[i];
			for (unsigned j = 0; j < face.mNumIndices; ++j) {
				indices.push_back(baseVertex + face.mIndices[j]);
			}
		}
	}

	mTriangleMesh = PhysicsEngine::get()->cookTriangleMesh(points, indices);
}
