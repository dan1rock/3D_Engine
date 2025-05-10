#pragma once
#include <d3d11.h>

class GraphicsEngine;
class DeviceContext;

class VertexShader
{
public:
	VertexShader();
	bool release();
	~VertexShader();
private:
	bool init(const void* shaderBytecode, SIZE_T bytecodeLength);

	ID3D11VertexShader* mVertexShader = nullptr;

	friend class GraphicsEngine;
	friend class DeviceContext;
};
