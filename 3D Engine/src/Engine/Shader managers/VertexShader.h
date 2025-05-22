#pragma once
#include <d3d11.h>

class GraphicsEngine;
class DeviceContext;

class VertexShader
{
public:
	VertexShader();
	// Звільняє ресурси вершинного шейдера
	bool release();
	~VertexShader();
private:
	// Ініціалізує вершинний шейдер з байткоду
	bool init(const void* shaderBytecode, SIZE_T bytecodeLength);

	ID3D11VertexShader* mVertexShader = nullptr;

	friend class GraphicsEngine;
	friend class DeviceContext;
};
