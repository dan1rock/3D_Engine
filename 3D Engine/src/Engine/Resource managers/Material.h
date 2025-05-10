#pragma once
#include <vector>

class PixelShader;
class VertexShader;
class Texture;
class ConstantBuffer;

class Material
{
public:
	Material();
	~Material();

	void setVertexShader(VertexShader* vertexShader);
	void setPixelShader(PixelShader* pixelShader);
	void addTexture(Texture* texture);
	void removeTexture(unsigned int id);

	void setColor(float r, float g, float b, float a);

	bool cullBack = true;
	bool clampTexture = true;
	float ambient = 0.4f;
	float smoothness = 0.5f;
	float shininess = 32.0f;
	float textureScale = 1.0f;
	float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	bool dontDeleteOnLoad = false;

	void setConstantBuffer(ConstantBuffer* constantBuffer, int slot = 0);

private:
	void onMaterialSet();

	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;
	ConstantBuffer* mConstantBuffer = nullptr;

	ConstantBuffer* mConstantBuffers[4] = {};

	std::vector<Texture*> mTextures;

	friend class GraphicsEngine;
};

