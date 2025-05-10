#pragma once
#include <d3d11.h>

class GraphicsEngine;
class DeviceContext;

class PixelShader
{
public:
	PixelShader();
	bool release();
	~PixelShader();
private:
	bool init(const void* shaderBytecode, SIZE_T bytecodeLength);

	ID3D11PixelShader* mPixelShader = nullptr;

	friend class GraphicsEngine;
	friend class DeviceContext;
};
