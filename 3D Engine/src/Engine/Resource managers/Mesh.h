#pragma once
#include "Resource.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

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

private:
	VertexBuffer* mVertexBuffer = nullptr;
	IndexBuffer* mIndexBuffer = nullptr;
};
