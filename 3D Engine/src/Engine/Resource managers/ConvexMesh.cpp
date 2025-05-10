#include "ConvexMesh.h"
#include "PhysicsEngine.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Vector3.h"
#include "Vector2.h"
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

void* ConvexMesh::getConvexMesh()
{
	if (mConvexMesh == nullptr) createConvexMesh();
    return mConvexMesh;
}

void* ConvexMesh::getTriangleMesh()
{
	if (mTriangleMesh == nullptr) createTriangleMesh();
	return mTriangleMesh;
}

void ConvexMesh::createConvexMesh()
{
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

	std::vector<PxVec3> points;

	for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[m];

		for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
			points.emplace_back(PxVec3(
				mesh->mVertices[i].x,
				mesh->mVertices[i].y,
				mesh->mVertices[i].z));
		}
	}

	mConvexMesh = PhysicsEngine::get()->cookConvexMesh(points);
}

void ConvexMesh::createTriangleMesh()
{
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

	std::vector<PxVec3> points;
	std::vector<PxU32> indices;

	for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
		aiMesh* mesh = scene->mMeshes[m];
		for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
			points.emplace_back(PxVec3(
				mesh->mVertices[i].x,
				mesh->mVertices[i].y,
				mesh->mVertices[i].z));
		}
		for (unsigned i = 0; i < mesh->mNumFaces; ++i) {
			const aiFace& face = mesh->mFaces[i];
			for (unsigned j = 0; j < face.mNumIndices; ++j) {
				indices.push_back(face.mIndices[j]);
			}
		}
	}

	mTriangleMesh = PhysicsEngine::get()->cookTriangleMesh(points, indices);
}
