#pragma once
#include "Matrix.h"

class Material;
class ConstantBuffer;

__declspec(align(16))
struct constant {
	Matrix world;
	Matrix model;
	Matrix view;
	Matrix projection;
	float cameraPos[4];
	float lightPos[4];
	float lightColor[4];
	unsigned int time;
};

class GlobalResources
{
public:
	GlobalResources();
	void init();
	~GlobalResources();

	void updateConstantBuffer();

	constant* getConstantData();
	ConstantBuffer* getConstantBuffer();
	Material* getDefaultMaterial();

private:
	constant mConstantData = {};
	ConstantBuffer* mConstantBuffer = nullptr;
	Material* mDefaultMaterial = nullptr;
};

