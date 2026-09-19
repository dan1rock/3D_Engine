#include "Mesh.h"
#include "GraphicsEngine.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Vector3.h"
#include "Vector2.h"
#include <vector>
#include <iostream>
#include <cfloat>

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

	// Рахуємо підсумковий розмір заздалегідь, щоб уникнути перевиділень пам'яті на великих моделях
    size_t totalVertices = 0;
    size_t totalIndices = 0;

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        totalVertices += scene->mMeshes[m]->mNumVertices;
        totalIndices += scene->mMeshes[m]->mNumFaces * 3;
    }

    outVertices.reserve(totalVertices);
    outIndices.reserve(totalIndices);
    mPositions.reserve(totalVertices);
    mIndices.reserve(totalIndices);

	// Межі меша рахуємо тут же, щоб не проходити по вершинах удруге
    Vector3 minPoint(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 maxPoint(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// Проходимо по всіх сітках сцени та збираємо вершини та індекси
	// Імена матеріалів з файлу дозволяють звертатися до слотів за назвою, а не за номером
    mMaterialNames.resize(scene->mNumMaterials);

    for (unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        aiString materialName;

        if (scene->mMaterials[i]->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS)
        {
            mMaterialNames[i] = materialName.C_Str();
        }
    }

    mSubMeshes.reserve(scene->mNumMeshes);

    for (unsigned m = 0; m < scene->mNumMeshes; ++m) {
        aiMesh* mesh = scene->mMeshes[m];

        uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());

		// Assimp розбиває модель на окремі сітки за матеріалами, тому кожна стає частиною меша
        SubMesh subMesh;
        subMesh.indexStart = static_cast<unsigned int>(outIndices.size());
        subMesh.materialSlot = mesh->mMaterialIndex;

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

            if (v.pos.x < minPoint.x) minPoint.x = v.pos.x;
            if (v.pos.y < minPoint.y) minPoint.y = v.pos.y;
            if (v.pos.z < minPoint.z) minPoint.z = v.pos.z;
            if (v.pos.x > maxPoint.x) maxPoint.x = v.pos.x;
            if (v.pos.y > maxPoint.y) maxPoint.y = v.pos.y;
            if (v.pos.z > maxPoint.z) maxPoint.z = v.pos.z;

            outVertices.push_back(v);
            mPositions.push_back(v.pos);
        }

        for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
            const aiFace& face = mesh->mFaces[f];
            for (unsigned j = 0; j < face.mNumIndices; ++j) {
                outIndices.push_back(baseVertex + face.mIndices[j]);
                mIndices.push_back(baseVertex + face.mIndices[j]);
            }
        }

        subMesh.indexCount = static_cast<unsigned int>(outIndices.size()) - subMesh.indexStart;

		// Порожні частини не створюють викликів малювання
        if (subMesh.indexCount > 0)
        {
            mSubMeshes.push_back(subMesh);

            if (subMesh.materialSlot + 1 > mMaterialCount) mMaterialCount = subMesh.materialSlot + 1;
        }
    }

	// Сфера навколо прямокутника меж гарантовано охоплює меш, тому відсікання нічого не втрачає
    if (!mPositions.empty())
    {
        mBoundsCenter = (minPoint + maxPoint) * 0.5f;
        mBoundsRadius = (maxPoint - minPoint).length() * 0.5f;
    }

	// Якщо бібліотеки матеріалів немає поруч з моделлю, Assimp зводить усі частини до одного
	// матеріалу. Тоді роздаємо кожній частині власний слот, щоб їх можна було задавати окремо
    bool hasDistinctSlots = false;

    for (const SubMesh& subMesh : mSubMeshes)
    {
        if (subMesh.materialSlot != mSubMeshes[0].materialSlot)
        {
            hasDistinctSlots = true;
            break;
        }
    }

    if (!hasDistinctSlots && mSubMeshes.size() > 1)
    {
        for (unsigned int i = 0; i < (unsigned int)mSubMeshes.size(); ++i)
        {
            mSubMeshes[i].materialSlot = i;
        }

        mMaterialCount = (unsigned int)mSubMeshes.size();
        mMaterialNames.assign(mMaterialCount, std::string());
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

// Повертає позиції вершин, збережені для побудови фізичних мешів
const std::vector<Vector3>& Mesh::getPositions() const
{
    return mPositions;
}

// Повертає індекси вершин, збережені для побудови фізичних мешів
const std::vector<unsigned int>& Mesh::getIndices() const
{
    return mIndices;
}

// Повертає частини меша, кожна з яких малюється своїм матеріалом
const std::vector<SubMesh>& Mesh::getSubMeshes() const
{
    return mSubMeshes;
}

// Повертає кількість слотів матеріалів, які використовує меш
unsigned int Mesh::getMaterialCount() const
{
    return mMaterialCount;
}

// Повертає ім'я матеріалу з файлу моделі, щоб слот можна було знайти не за номером
const std::string& Mesh::getMaterialName(unsigned int slot) const
{
    static const std::string empty;

    if (slot >= mMaterialNames.size()) return empty;

    return mMaterialNames[slot];
}

// Повертає номер слота за іменем матеріалу з файлу моделі
unsigned int Mesh::getMaterialSlot(const std::string& name) const
{
    for (unsigned int i = 0; i < (unsigned int)mMaterialNames.size(); ++i)
    {
        if (mMaterialNames[i] == name) return i;
    }

    return 0;
}

// Повертає центр сфери, що охоплює меш, у локальних координатах
const Vector3& Mesh::getBoundsCenter() const
{
    return mBoundsCenter;
}

// Повертає радіус сфери, що охоплює меш, у локальних координатах
float Mesh::getBoundsRadius() const
{
    return mBoundsRadius;
}
