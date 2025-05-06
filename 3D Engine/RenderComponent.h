#pragma once
#include "GraphicsEngine.h"
#include "DeviceContext.h"
#include "Mesh.h"
#include "Component.h"

class ConstantBuffer;
class Material;
class Matrix;

class RenderComponent : public Component
{
public:
	RenderComponent();
	~RenderComponent() override;

	virtual void render() = 0;
	void setMaterial(Material* material);
	void setMesh(Mesh* mesh);
	Mesh* getMesh();

protected:
	void registerComponent() override;

	virtual void awake() override;

	Material* mMaterial = nullptr;

	Mesh* mMesh = nullptr;

	bool isInitialized = false;
};

