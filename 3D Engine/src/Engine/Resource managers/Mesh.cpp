#include "Mesh.h"
#include "GraphicsEngine.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Vector3.h"
#include "Vector2.h"
#include <vector>
#include <iostream>

// Структура для зберігання вершинних даних
struct vertex {
    Vector3 pos;
    Vector3 normal;
    Vector2 texCoord;
};

// Завантажує модель з файлу, створює вершинний та індексний буфери
Mesh::Mesh(const wchar_t* fullPath) : Resource(fullPath)
{
    std::wstring ws(fullPath);
    std::string filePath(ws.begin(), ws.end());

	// Використовує Assimp для імпорту моделі з файлу
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_FlipUVs
        | aiProcess_JoinIdenticalVertices);

	// Перевіряємо, чи вдалося завантажити сцену
    if (!scene || !scene->HasMeshes())
    {
        std::cout << "Empty Scene" << std::endl;
        return;
    }

    std::vector<vertex> outVertices;
    std::vector<uint32_t> outIndices;

	// Проходимо по всіх сітках сцени та збираємо вершини та індекси
    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];

        uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());

        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            vertex v;

            v.pos.x = mesh->mVertices[i].x;
            v.pos.y = mesh->mVertices[i].y;
            v.pos.z = mesh->mVertices[i].z;

            v.normal.x = mesh->mNormals[i].x;
            v.normal.y = mesh->mNormals[i].y;
            v.normal.z = mesh->mNormals[i].z;

            if (mesh->mTextureCoords[0]) {
                v.texCoord.x = mesh->mTextureCoords[0][i].x;
                v.texCoord.y = mesh->mTextureCoords[0][i].y;
            }
            else {
                v.texCoord = Vector2(0, 0);
            }

            outVertices.push_back(v);
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned j = 0; j < face.mNumIndices; ++j) {
                outIndices.push_back(baseVertex + face.mIndices[j]);
            }
        }
    }

	// Створюємо вершинний та індексний буфери в графічному рушії
    mVertexBuffer = GraphicsEngine::get()->createVertexBuffer();
    mIndexBuffer = GraphicsEngine::get()->createIndexBuffer();

    void* shaderByteCode = nullptr;
    SIZE_T shaderSize = 0;

	// Компілюємо вершинний шейдер для лейауту вершинного буфера
    GraphicsEngine::get()->compileVertexShader(L"src\\Shaders\\VertexLayoutShader.hlsl", "main", &shaderByteCode, &shaderSize);

	// Завантажуємо вершинний та індексний буфери з даними
    mVertexBuffer->load(outVertices.data(), sizeof(vertex), outVertices.size(), shaderByteCode, shaderSize);
    mIndexBuffer->load(outIndices.data(), outIndices.size());

	// Звільняємо ресурси вершинного шейдера
    GraphicsEngine::get()->releaseVertexShader();
}

// Звільняє ресурси вершинного та індексного буферів
Mesh::~Mesh()
{
	if (mVertexBuffer)
	{
		mVertexBuffer->release();
		mVertexBuffer = nullptr;
	}
	if (mIndexBuffer)
	{
		mIndexBuffer->release();
		mIndexBuffer = nullptr;
	}
}

// Повертає вказівник на вершинний буфер
VertexBuffer* Mesh::getVertexBuffer()
{
    return mVertexBuffer;
}

// Повертає вказівник на індексний буфер
IndexBuffer* Mesh::getIndexBuffer()
{
    return mIndexBuffer;
}
