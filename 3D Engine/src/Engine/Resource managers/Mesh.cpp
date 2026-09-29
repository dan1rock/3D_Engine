#include "Mesh.h"
#include "GraphicsEngine.h"
#include <assimp/cimport.h>
#include <assimp/config.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "Vector3.h"
#include "Vector2.h"
#include "Matrix.h"
#include <vector>
#include <iostream>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <initializer_list>
#include <fstream>
#include <algorithm>
#include <cwctype>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Структура для зберігання вершинних даних
struct vertex {
    Vector3 pos;
    Vector3 normal;
    Vector2 texCoord;
};

// Сітка моделі разом з повним перетворенням вузла, у якому вона стоїть
struct MeshInstance
{
    unsigned int mesh;
    aiMatrix4x4 transform;
};

// Обходить вузли моделі й збирає їхні сітки з перетвореннями від кореня
static void collectInstances(const aiNode* node, const aiMatrix4x4& parent, std::vector<MeshInstance>& out)
{
    if (node == nullptr) return;

    aiMatrix4x4 transform = parent * node->mTransformation;

    for (unsigned i = 0; i < node->mNumMeshes; ++i) out.push_back({ node->mMeshes[i], transform });

    for (unsigned c = 0; c < node->mNumChildren; ++c) collectInstances(node->mChildren[c], transform, out);
}

// Повертає шлях до текстури першого знайденого типу: вбудовану як модель::*номер, зовнішню - повним шляхом до файлу
static std::wstring findTexture(const aiScene* scene, const aiMaterial* material, std::initializer_list<aiTextureType> types, const std::wstring& modelPath)
{
    for (aiTextureType type : types)
    {
        aiString path;

        if (material->GetTexture(type, 0, &path) != AI_SUCCESS || path.length == 0) continue;

		// Вбудована у файл моделі текстура читається з пам'яті за її номером
        if (const aiTexture* embedded = scene->GetEmbeddedTexture(path.C_Str()))
        {
            for (unsigned t = 0; t < scene->mNumTextures; ++t)
            {
                if (scene->mTextures[t] == embedded) return modelPath + L"::*" + std::to_wstring(t);
            }
        }

		// Зовнішню текстуру спершу шукають у проєкті, поруч з моделлю й у теці текстур, і лише потім за записаним шляхом
        std::string raw = path.C_Str();
        std::wstring wide(raw.begin(), raw.end());
        std::wstring file = wide.substr(wide.find_last_of(L"\\/") + 1);
        std::wstring folder = modelPath.substr(0, modelPath.find_last_of(L"\\/") + 1);

        for (const std::wstring& candidate : { folder + wide, folder + file, std::wstring(L"Assets\\Textures\\") + file, wide })
        {
            std::error_code error;

            if (filesystem::is_regular_file(candidate, error)) return filesystem::absolute(candidate).wstring();
        }

        std::cout << "Texture " << raw << " used by model material is not found" << std::endl;
    }

    return std::wstring();
}

// Перевіряє, чи OBJ-файл має свою бібліотеку матеріалів; без неї usemtl дає лише імена без жодних налаштувань
static bool objHasMaterialLibrary(const std::wstring& modelPath)
{
    std::ifstream file(modelPath);
    std::string line;

	// Бібліотеку оголошують на початку файлу, тож увесь великий файл читати не потрібно
    for (int i = 0; i < 2000 && std::getline(file, line); ++i)
    {
        if (line.compare(0, 7, "mtllib ") != 0) continue;

        std::string name = line.substr(7);

        while (!name.empty() && (name.back() == '\r' || name.back() == ' ')) name.pop_back();

        std::wstring folder = modelPath.substr(0, modelPath.find_last_of(L"\\/") + 1);
        std::error_code error;

        return filesystem::is_regular_file(folder + std::wstring(name.begin(), name.end()), error);
    }

    return false;
}

// Читає матеріали й вбудовані текстури моделі; колір, карти й прозорість переходять у налаштування матеріалу
static void readModelMaterials(const aiScene* scene, const std::wstring& modelPath, std::vector<ModelMaterial>& materials, std::vector<EmbeddedTexture>& textures)
{
	// OBJ без бібліотеки матеріалів нічого про матеріали не описує
    std::wstring extension = modelPath.substr(modelPath.find_last_of(L'.') + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::towlower);

    bool describesMaterials = extension != L"obj" || objHasMaterialLibrary(modelPath);

    for (unsigned t = 0; t < scene->mNumTextures; ++t)
    {
        const aiTexture* source = scene->mTextures[t];

        EmbeddedTexture texture;
        texture.name = source->mFilename.C_Str();

		// Нульова висота означає стиснений файл зображення довжиною mWidth байтів
        if (source->mHeight == 0)
        {
            const unsigned char* bytes = (const unsigned char*)source->pcData;
            texture.data.assign(bytes, bytes + source->mWidth);
            texture.format = source->achFormatHint;
        }
        else
        {
            texture.width = source->mWidth;
            texture.height = source->mHeight;
            texture.data.resize((size_t)source->mWidth * source->mHeight * 4);
            memcpy(texture.data.data(), source->pcData, texture.data.size());
        }

        textures.push_back(texture);
    }

    materials.resize(scene->mNumMaterials);

    for (unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        const aiMaterial* source = scene->mMaterials[i];
        ModelMaterial& material = materials[i];
        MaterialProperties& p = material.properties;

        aiString name;
        if (source->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) material.name = name.C_Str();

        material.maps[(int)MaterialMap::Base] = findTexture(scene, source, { aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE }, modelPath);
        material.maps[(int)MaterialMap::Normal] = findTexture(scene, source, { aiTextureType_NORMAL_CAMERA, aiTextureType_NORMALS }, modelPath);
        material.maps[(int)MaterialMap::Metallic] = findTexture(scene, source, { aiTextureType_METALNESS }, modelPath);
        material.maps[(int)MaterialMap::Occlusion] = findTexture(scene, source, { aiTextureType_AMBIENT_OCCLUSION, aiTextureType_LIGHTMAP }, modelPath);
        material.maps[(int)MaterialMap::Emission] = findTexture(scene, source, { aiTextureType_EMISSION_COLOR, aiTextureType_EMISSIVE }, modelPath);

        bool hasMaps = false;

        for (const std::wstring& map : material.maps)
        {
            if (!map.empty()) hasMaps = true;
        }

		// Матеріал, який імпортер підставив сам, модель не описує
        material.imported = describesMaterials && (hasMaps || material.name != AI_DEFAULT_MATERIAL_NAME);

		// З основною картою колір лишається білим, щоб текстура мала свої кольори
        aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
        bool hasColor = source->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS || source->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS;

        if (hasColor && material.maps[(int)MaterialMap::Base].empty())
        {
            p.baseColor[0] = color.r;
            p.baseColor[1] = color.g;
            p.baseColor[2] = color.b;
        }

		// Частково прозорий матеріал стає прозорою поверхнею; нульову непрозорість деякі редактори пишуть помилково, тож її не беремо
        float opacity = 1.0f;

        if (source->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS && opacity > 0.01f && opacity < 0.99f)
        {
            p.baseColor[3] = opacity;
            p.surface = SurfaceType::Transparent;
        }

        aiColor3D emissive(0.0f, 0.0f, 0.0f);
        source->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);

        if (!emissive.IsBlack() || !material.maps[(int)MaterialMap::Emission].empty())
        {
            p.emission = true;

			// Карта випромінювання без кольору світиться своїми кольорами
            bool black = emissive.IsBlack();
            p.emissionColor[0] = black ? 1.0f : emissive.r;
            p.emissionColor[1] = black ? 1.0f : emissive.g;
            p.emissionColor[2] = black ? 1.0f : emissive.b;
        }

        float metallic = 0.0f;
        if (source->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS) p.metallic = metallic;

        float roughness = 0.5f;
        if (source->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) p.smoothness = 1.0f - roughness;

        int twoSided = 0;
        if (source->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided) p.renderFace = RenderFace::Both;
    }

	// Однакові чи порожні імена доповнюються номером матеріалу, щоб посилання вказувало на один матеріал
    std::vector<std::string> keys;

    for (unsigned i = 0; i < (unsigned)materials.size(); ++i)
    {
        if (!materials[i].imported) continue;

        std::string key = materials[i].name.empty() ? std::string("Material") : materials[i].name;

        if (std::find(keys.begin(), keys.end(), key) != keys.end()) key += " " + std::to_string(i);

        keys.push_back(key);
        materials[i].key = key;
    }
}

// Читає сцену через C-інтерфейс бібліотеки, щоб одна її збірка працювала з обома збірками рушія; одиниці файлу переводяться в метри
static const aiScene* importScene(const std::string& path)
{
    aiPropertyStore* store = aiCreatePropertyStore();

	// Допоміжні вузли півотів FBX зливаються з вузлами частин, тож ієрархія лишається такою, як у редакторі моделей
    aiSetImportPropertyInteger(store, AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, 0);

    const aiScene* scene = aiImportFileExWithProperties(
        path.c_str(),
        aiProcess_Triangulate
        | aiProcess_GenSmoothNormals
        | aiProcess_FlipUVs
        | aiProcess_JoinIdenticalVertices
        | aiProcess_GlobalScale,
        nullptr,
        store);

    aiReleasePropertyStore(store);

    return scene;
}

// Модель, прочитана заради її окремих вузлів: вузли однієї моделі завантажуються поспіль, тож файл читається лише раз
static std::string gCachedPath;
static const aiScene* gCachedScene = nullptr;

// Звільняє модель, збережену для завантаження її окремих вузлів
void Mesh::releaseImportCache()
{
    if (gCachedScene) aiReleaseImport(gCachedScene);

    gCachedScene = nullptr;
    gCachedPath.clear();
}

// Збирає вузли моделі в глибину, батька раніше за дітей, разом з номером батька кожного
static void collectNodes(const aiNode* node, int parent, std::vector<const aiNode*>& nodes, std::vector<int>& parents)
{
    if (node == nullptr) return;

    int index = (int)nodes.size();

    nodes.push_back(node);
    parents.push_back(parent);

    for (unsigned c = 0; c < node->mNumChildren; ++c) collectNodes(node->mChildren[c], index, nodes, parents);
}

// Повертає ключі вузлів: ім'я вузла, а для повторів ім'я з номером
static std::vector<std::string> nodeKeys(const std::vector<const aiNode*>& nodes)
{
    std::vector<std::string> keys;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        std::string key = nodes[i]->mName.length > 0 ? nodes[i]->mName.C_Str() : "Node";

        if (std::find(keys.begin(), keys.end(), key) != keys.end()) key += " (" + std::to_string(i) + ")";

        keys.push_back(key);
    }

    return keys;
}

// Повертає рівномірний масштаб кореня, яким бібліотека перевела одиниці файлу в метри, або 1
static float unitScale(const aiScene* scene)
{
    aiVector3D scaling;
    aiQuaternion rotation;
    aiVector3D position;

    scene->mRootNode->mTransformation.Decompose(scaling, rotation, position);

    bool uniform = std::fabs(scaling.x - scaling.y) <= 1e-4f * std::fabs(scaling.x) && std::fabs(scaling.x - scaling.z) <= 1e-4f * std::fabs(scaling.x);

    return uniform && scaling.x > 0.0f ? scaling.x : 1.0f;
}

// Розкладає перетворення вузла на положення, поворот і масштаб так, як їх складає трансформація рушія
static void decompose(const aiMatrix4x4& source, Vector3& position, Vector3& rotation, Vector3& scale)
{
	// Рушій множить рядок вершини на матрицю, тож його матриця - транспонована матриця бібліотеки
    Matrix matrix;

    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column) matrix.mat[row][column] = source[column][row];
    }

    position = matrix.getTranslation();
    rotation = matrix.getRotation();
    scale = matrix.getScale();
}

// Завантажує модель з файлу, створює вершинний та індексний буфери; шлях модель::вузол бере лише сітки цього вузла
Mesh::Mesh(const wchar_t* fullPath) : Resource(fullPath)
{
    std::wstring requested(fullPath);
    size_t separator = requested.find(L"::");

    std::wstring ws = separator == std::wstring::npos ? requested : requested.substr(0, separator);
    std::wstring wideKey = separator == std::wstring::npos ? std::wstring() : requested.substr(separator + 2);
    std::string nodeKey(wideKey.begin(), wideKey.end());
    std::string filePath(ws.begin(), ws.end());

	// Вузол береться з моделі, яку для вузлів уже прочитали; сама модель читається щоразу окремо
    const aiScene* scene = nullptr;

    if (nodeKey.empty())
    {
        scene = importScene(filePath);
    }
    else
    {
        if (gCachedPath != filePath || gCachedScene == nullptr)
        {
            releaseImportCache();

            gCachedScene = importScene(filePath);
            gCachedPath = filePath;
        }

        scene = gCachedScene;
    }

	// Прочитана для самої моделі сцена звільняється за будь-якого виходу з конструктора, зокрема й через виняток
    struct SceneGuard
    {
        const aiScene* scene;
        bool owned;
        ~SceneGuard() { if (owned && scene) aiReleaseImport(scene); }
    } guard = { scene, nodeKey.empty() };

	// Перевіряємо, чи вдалося завантажити сцену
    if (!scene || !scene->HasMeshes())
    {
        std::cout << "Empty Scene: " << filePath << std::endl;

		// Кидаємо виняток, а не просто виходимо: інакше менеджер закешував би меш без буферів,
		// і перша ж спроба його намалювати впала б на розіменуванні nullptr
		throw std::runtime_error("Mesh file is missing or has no geometry");
    }

    std::vector<vertex> outVertices;
    std::vector<uint32_t> outIndices;

    std::vector<const aiNode*> nodes;
    std::vector<int> parents;
    collectNodes(scene->mRootNode, -1, nodes, parents);

    std::vector<std::string> keys = nodeKeys(nodes);
    float scale = unitScale(scene);

    std::vector<MeshInstance> instances;

    if (nodeKey.empty())
    {
		// Сітки беруться з вузлів моделі разом з їхнім розташуванням, бо у FBX частини стоять у вузлах зі зсувом і поворотом
        collectInstances(scene->mRootNode, aiMatrix4x4(), instances);

		// Модель без вузлів малює свої сітки як є
        if (instances.empty())
        {
            for (unsigned m = 0; m < scene->mNumMeshes; ++m) instances.push_back({ m, aiMatrix4x4() });
        }

		// Ієрархія вузлів для створення об'єктів моделі; переведення в метри лишається в сітках і зсувах, а корінь має масштаб 1
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            ModelNode node;
            node.name = nodes[i]->mName.C_Str();
            node.key = keys[i];
            node.parent = parents[i];
            node.hasMeshes = nodes[i]->mNumMeshes > 0;

            decompose(nodes[i]->mTransformation, node.position, node.rotation, node.scale);

            if (i == 0) node.scale = Vector3(node.scale.x / scale, node.scale.y / scale, node.scale.z / scale);
            else node.position = Vector3(node.position.x * scale, node.position.y * scale, node.position.z * scale);

            mModelNodes.push_back(node);
        }
    }
    else
    {
		// Сітки вузла лишаються у просторі самого вузла, лише в метрах
        auto found = std::find(keys.begin(), keys.end(), nodeKey);

        if (found == keys.end()) throw std::runtime_error("Model has no such node");

        const aiNode* node = nodes[found - keys.begin()];

        aiMatrix4x4 unit;
        aiMatrix4x4::Scaling(aiVector3D(scale, scale, scale), unit);

        for (unsigned i = 0; i < node->mNumMeshes; ++i) instances.push_back({ node->mMeshes[i], unit });

        if (instances.empty()) throw std::runtime_error("Model node has no geometry");
    }

	// Рахуємо підсумковий розмір заздалегідь, щоб уникнути перевиділень пам'яті на великих моделях
    size_t totalVertices = 0;
    size_t totalIndices = 0;

    for (const MeshInstance& instance : instances) {
        totalVertices += scene->mMeshes[instance.mesh]->mNumVertices;
        totalIndices += scene->mMeshes[instance.mesh]->mNumFaces * 3;
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

    mSubMeshes.reserve(instances.size());

    for (const MeshInstance& instance : instances) {
        aiMesh* mesh = scene->mMeshes[instance.mesh];

		// Нормалі повертаються разом з вузлом, але без його масштабу
        aiMatrix3x3 normalMatrix = aiMatrix3x3(instance.transform);
        normalMatrix.Inverse().Transpose();

        uint32_t baseVertex = static_cast<uint32_t>(outVertices.size());

		// Assimp розбиває модель на окремі сітки за матеріалами, тому кожна стає частиною меша
        SubMesh subMesh;
        subMesh.indexStart = static_cast<unsigned int>(outIndices.size());
        subMesh.materialSlot = mesh->mMaterialIndex;

        for (unsigned i = 0; i < mesh->mNumVertices; ++i) {
            vertex v;

            aiVector3D position = instance.transform * mesh->mVertices[i];
            aiVector3D normal = normalMatrix * mesh->mNormals[i];
            normal.Normalize();

            v.pos.x = position.x;
            v.pos.y = position.y;
            v.pos.z = position.z;

            v.normal.x = normal.x;
            v.normal.y = normal.y;
            v.normal.z = normal.z;

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

	// Матеріали моделі читаються до поділу на слоти: справжні матеріали з файлу лишають частини так, як їх задумали
    readModelMaterials(scene, ws, mModelMaterials, mEmbeddedTextures);

	// Сітки одного вузла отримують слоти підряд, у порядку своїх матеріалів
    if (!nodeKey.empty())
    {
        std::vector<int> slotOf(scene->mNumMaterials, -1);
        std::vector<ModelMaterial> materials;
        std::vector<std::string> names;

        for (SubMesh& subMesh : mSubMeshes)
        {
            unsigned int index = subMesh.materialSlot;

            if (index < slotOf.size() && slotOf[index] < 0)
            {
                slotOf[index] = (int)materials.size();
                materials.push_back(index < mModelMaterials.size() ? mModelMaterials[index] : ModelMaterial());
                names.push_back(index < mMaterialNames.size() ? mMaterialNames[index] : std::string());
            }

            subMesh.materialSlot = index < slotOf.size() ? (unsigned int)slotOf[index] : 0;
        }

        mModelMaterials = materials;
        mMaterialNames = names;
        mMaterialCount = materials.empty() ? 1 : (unsigned int)materials.size();
    }

    bool anyImported = false;

    for (const SubMesh& subMesh : mSubMeshes)
    {
        if (subMesh.materialSlot < mModelMaterials.size() && mModelMaterials[subMesh.materialSlot].imported) anyImported = true;
    }

	// Модель без власних матеріалів отримує окремий слот для кожної частини, щоб їх можна було задавати окремо
    if (!anyImported && !mSubMeshes.empty())
    {
        mModelMaterials.clear();

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

// Повертає матеріали з файлу моделі за слотами; порожній список, якщо модель їх не описує
const std::vector<ModelMaterial>& Mesh::getModelMaterials() const
{
    return mModelMaterials;
}

// Повертає вбудовані у файл моделі текстури
const std::vector<EmbeddedTexture>& Mesh::getEmbeddedTextures() const
{
    return mEmbeddedTextures;
}

// Повертає вбудовану текстуру за назвою з посилання, як-от *0, або nullptr
const EmbeddedTexture* Mesh::findEmbeddedTexture(const std::wstring& name) const
{
    if (name.size() < 2 || name[0] != L'*') return nullptr;

    unsigned int index = (unsigned int)_wtoi(name.c_str() + 1);

    return index < mEmbeddedTextures.size() ? &mEmbeddedTextures[index] : nullptr;
}

// Повертає вузли ієрархії моделі, батьки раніше за дітей; порожній список у меша окремого вузла
const std::vector<ModelNode>& Mesh::getModelNodes() const
{
    return mModelNodes;
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
