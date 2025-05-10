#pragma once
#include "RenderComponent.h"

class MeshRenderer : public RenderComponent
{
public:
	MeshRenderer();
	MeshRenderer(Mesh* mesh);
	MeshRenderer(Mesh* mesh, Material* material);

	~MeshRenderer() override;

	virtual void render() override;

protected:
	MeshRenderer* instantiate() const override {
		return new MeshRenderer(*this);
	};

private:
	virtual void awake() override;
};
