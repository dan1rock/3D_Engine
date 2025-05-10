#pragma once
#include "Resource.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

class Mesh : public Resource
{
public:
	Mesh(const wchar_t* fullPath);
	~Mesh();

	VertexBuffer* getVertexBuffer();
	IndexBuffer* getIndexBuffer();

private:
	VertexBuffer* mVertexBuffer = nullptr;
	IndexBuffer* mIndexBuffer = nullptr;
};
