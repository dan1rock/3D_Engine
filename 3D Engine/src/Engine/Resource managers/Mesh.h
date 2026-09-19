#pragma once
#include "Resource.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Vector3.h"
#include <vector>
#include <string>

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
	// Завантажує модель з файлу, створює вершинний та індексний буфери
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

	// Сфера, що охоплює меш; за нею виконується відсікання за пірамідою видимості
	Vector3 mBoundsCenter = {};
	float mBoundsRadius = 0.0f;

	// Копія геометрії в оперативній пам'яті, щоб фізика не читала той самий файл вдруге
	std::vector<Vector3> mPositions;
	std::vector<unsigned int> mIndices;
};
