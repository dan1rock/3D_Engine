#pragma once
#include <vector>

class PixelShader;
class VertexShader;
class Texture;
class ConstantBuffer;

class Material
{
public:
	// Ініціалізує шейдери, константні буфери та реєструє матеріал
	Material();
	// Звільняє ресурси та знімає реєстрацію матеріалу
	~Material();

	// Встановлює вершинний шейдер для матеріалу
	void setVertexShader(VertexShader* vertexShader);
	// Встановлює піксельний шейдер для матеріалу
	void setPixelShader(PixelShader* pixelShader);
	// Додає текстуру до матеріалу
	void addTexture(Texture* texture);
	// Видаляє текстуру з матеріалу за індексом
	void removeTexture(unsigned int id);

	// Встановлює колір матеріалу
	void setColor(float r, float g, float b, float a);

	bool cullBack = true;
	bool clampTexture = true;
	float ambient = 0.4f;
	float smoothness = 0.5f;
	float shininess = 32.0f;
	float textureScale = 1.0f;
	float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	bool dontDeleteOnLoad = false;
	
	// Встановлює константний буфер для матеріалу у відповідний слот
	void setConstantBuffer(ConstantBuffer* constantBuffer, int slot = 0);

private:
	// Оновлює дані матеріалу та встановлює константні буфери для шейдерів
	void onMaterialSet();

	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;
	ConstantBuffer* mConstantBuffer = nullptr;

	ConstantBuffer* mConstantBuffers[4] = {};

	std::vector<Texture*> mTextures;

	friend class GraphicsEngine;
};

