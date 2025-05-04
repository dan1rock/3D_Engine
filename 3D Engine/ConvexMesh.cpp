#include "ConvexMesh.h"
#include "PhysicsEngine.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Vector3.h"
#include "Vector2.h"
#include <vector>

ConvexMesh::ConvexMesh(const wchar_t* fullPath) : Resource(fullPath)
{
	std::wstring ws(fullPath);
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

ConvexMesh::~ConvexMesh()
{
}

void* ConvexMesh::getConvexMesh()
{
    return mConvexMesh;
}
