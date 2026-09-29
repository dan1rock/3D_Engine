#pragma once
#include "Resource.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Vector3.h"
#include "Material.h"
#include <vector>
#include <string>

// Матеріал, записаний у файлі моделі: ім'я, налаштування та шляхи до текстур його карт
struct ModelMaterial
{
	std::string name;
	// Незмінне в межах моделі ім'я, за яким на матеріал посилаються; однакові імена доповнено номером
	std::string key;
	MaterialProperties properties;
	std::wstring maps[(int)MaterialMap::Count];
	// Чи модель справді описує цей матеріал, а не підставлений імпортером порожній
	bool imported = false;
};

// Вузол ієрархії моделі: ім'я, ключ для посилання на його сітки, батько та розташування відносно батька
struct ModelNode
{
	std::string name;
	std::string key;
	int parent = -1;
	Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 rotation = Vector3(0.0f, 0.0f, 0.0f);
	Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
	bool hasMeshes = false;
};

// Текстура, вбудована у файл моделі: стиснений файл зображення або готові пікселі BGRA
struct EmbeddedTexture
{
	std::vector<unsigned char> data;
	// Для готових пікселів - розмір зображення; для стисненого файлу висота нульова
	unsigned int width = 0;
	unsigned int height = 0;
	// Розширення стисненого файлу, як-от png чи jpg
	std::string format;
	// Ім'я файлу, під яким текстуру вбудували, або порожнє
	std::string name;
};
// Частина меша з власним матеріалом: неперервний діапазон індексів у спільному буфері
struct SubMesh
{
	unsigned int indexStart = 0;
	unsigned int indexCount = 0;
	// Номер слота матеріалу, який належить цій частині
	unsigned int materialSlot = 0;
};

class Mesh : public Resource
{
public:
	// Завантажує модель з файлу, створює вершинний та індексний буфери; шлях модель::вузол бере лише сітки цього вузла
	Mesh(const wchar_t* fullPath);
	~Mesh();

	// Повертає вказівник на вершинний буфер
	VertexBuffer* getVertexBuffer();
	// Повертає вказівник на індексний буфер
	IndexBuffer* getIndexBuffer();

	// Повертає позиції вершин, збережені для побудови фізичних мешів
	const std::vector<Vector3>& getPositions() const;
	// Повертає індекси вершин, збережені для побудови фізичних мешів
	const std::vector<unsigned int>& getIndices() const;

	// Повертає частини меша, кожна з яких малюється своїм матеріалом
	const std::vector<SubMesh>& getSubMeshes() const;
	// Повертає кількість слотів матеріалів, які використовує меш
	unsigned int getMaterialCount() const;
	// Повертає ім'я матеріалу з файлу моделі, щоб слот можна було знайти не за номером
	const std::string& getMaterialName(unsigned int slot) const;
	// Повертає номер слота за іменем матеріалу з файлу моделі
	unsigned int getMaterialSlot(const std::string& name) const;
	// Повертає матеріали з файлу моделі за слотами; порожній список, якщо модель їх не описує
	const std::vector<ModelMaterial>& getModelMaterials() const;
	// Повертає вбудовані у файл моделі текстури
	const std::vector<EmbeddedTexture>& getEmbeddedTextures() const;
	// Повертає вбудовану текстуру за назвою з посилання, як-от *0, або nullptr
	const EmbeddedTexture* findEmbeddedTexture(const std::wstring& name) const;
	// Повертає вузли ієрархії моделі, батьки раніше за дітей; порожній список у меша окремого вузла
	const std::vector<ModelNode>& getModelNodes() const;

	// Звільняє модель, збережену для завантаження її окремих вузлів
	static void releaseImportCache();

	// Повертає центр сфери, що охоплює меш, у локальних координатах
	const Vector3& getBoundsCenter() const;
	// Повертає радіус сфери, що охоплює меш, у локальних координатах
	float getBoundsRadius() const;

private:
	VertexBuffer* mVertexBuffer = nullptr;
	IndexBuffer* mIndexBuffer = nullptr;

	// Частини меша та кількість слотів матеріалів, знайдених у файлі
	std::vector<SubMesh> mSubMeshes;
	unsigned int mMaterialCount = 1;
	// Імена матеріалів у тому ж порядку, що й слоти
	std::vector<std::string> mMaterialNames;
	// Матеріали з файлу моделі за слотами та вбудовані в нього текстури
	std::vector<ModelMaterial> mModelMaterials;
	std::vector<EmbeddedTexture> mEmbeddedTextures;
	// Вузли ієрархії моделі
	std::vector<ModelNode> mModelNodes;

	// Сфера, що охоплює меш; за нею виконується відсікання за пірамідою видимості
	Vector3 mBoundsCenter = {};
	float mBoundsRadius = 0.0f;

	// Копія геометрії в оперативній пам'яті, щоб фізика не читала той самий файл вдруге
	std::vector<Vector3> mPositions;
	std::vector<unsigned int> mIndices;
};
