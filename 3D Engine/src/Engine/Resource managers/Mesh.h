#pragma once
#include "Resource.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Vector3.h"
#include <vector>

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

	// Повертає центр сфери, що охоплює меш, у локальних координатах
	const Vector3& getBoundsCenter() const;
	// Повертає радіус сфери, що охоплює меш, у локальних координатах
	float getBoundsRadius() const;

private:
	VertexBuffer* mVertexBuffer = nullptr;
	IndexBuffer* mIndexBuffer = nullptr;

	// Сфера, що охоплює меш; за нею виконується відсікання за пірамідою видимості
	Vector3 mBoundsCenter = {};
	float mBoundsRadius = 0.0f;

	// Копія геометрії в оперативній пам'яті, щоб фізика не читала той самий файл вдруге
	std::vector<Vector3> mPositions;
	std::vector<unsigned int> mIndices;
};
