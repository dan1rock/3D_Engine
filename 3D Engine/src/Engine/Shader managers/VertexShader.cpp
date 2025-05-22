#include "VertexShader.h"
#include "GraphicsEngine.h"

VertexShader::VertexShader()
{
}

// Звільняє ресурси вершинного шейдера
bool VertexShader::release()
{
	mVertexShader->Release();

	return true;
}

VertexShader::~VertexShader()
{
}

// Ініціалізує вершинний шейдер з байткоду
bool VertexShader::init(const void* shaderBytecode, SIZE_T bytecodeLength)
{
	if(FAILED(GraphicsEngine::get()->mD3dDevice->CreateVertexShader(shaderBytecode, bytecodeLength, NULL, &mVertexShader)))
		return false;

	return true;
}
