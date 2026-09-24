#pragma once
#include <vector>
#include <string>

class PixelShader;
class VertexShader;
class Texture;
class ConstantBuffer;

class Material
{
public:
	// Ініціалізує шейдери, константні буфери та реєструє матеріал
	Material();
	// Копіює налаштування матеріалу, створюючи копії власний константний буфер та реєстрацію
	Material(const Material& material);
	// Копіює налаштування іншого матеріалу, зберігаючи власний константний буфер
	Material& operator=(const Material& material);
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

	// Повертає шлях до першої текстури матеріалу, або порожній рядок, якщо текстур немає
	std::wstring getTexturePath() const;
	// Повертає шлях до піксельного шейдера матеріалу, щоб сцена не втрачала нестандартний шейдер
	std::wstring getPixelShaderPath() const;
	// Повертає кількість текстур матеріалу
	unsigned int getTextureCount() const;

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
	// Переносить налаштування іншого матеріалу, не торкаючись власних ресурсів
	void copySettings(const Material& material);

	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;
	ConstantBuffer* mConstantBuffer = nullptr;

	ConstantBuffer* mConstantBuffers[4] = {};

	std::vector<Texture*> mTextures;

	friend class GraphicsEngine;
};

