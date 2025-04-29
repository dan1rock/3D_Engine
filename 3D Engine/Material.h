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

	float ambient = 0.4f;
	float diffuse = 0.4f;
	float specular = 0.5f;
	float shininess = 32.0f;

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

