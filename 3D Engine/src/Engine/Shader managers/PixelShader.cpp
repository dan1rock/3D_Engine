#include "PixelShader.h"
#include "GraphicsEngine.h"

PixelShader::PixelShader()
{
}

// Звільняє ресурси піксельного шейдера
bool PixelShader::release()
{
	mPixelShader->Release();

	return true;
}

PixelShader::~PixelShader()
{
}

// Ініціалізує піксельний шейдер з байткоду
bool PixelShader::init(const void* shaderBytecode, SIZE_T bytecodeLength)
{
	if (FAILED(GraphicsEngine::get()->mD3dDevice->CreatePixelShader(shaderBytecode, bytecodeLength, NULL, &mPixelShader)))
		return false;

	return true;
}
