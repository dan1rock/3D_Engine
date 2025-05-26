#pragma once
#include "Renderer.h"

// Компонент для рендеру мешів
class MeshRenderer : public Renderer
{
public:
	MeshRenderer();
	// Конструктор класу MeshRenderer з вказаним мешем
	MeshRenderer(Mesh* mesh);
	// Конструктор класу MeshRenderer з вказаними мешем та матеріалом
	MeshRenderer(Mesh* mesh, Material* material);

	~MeshRenderer() override;

	// Викликається для рендеру об'єкта: встановлює буфери та викликає рендеринг
	virtual void render() override;

protected:
	// Конструктор копіювання
	MeshRenderer* instantiate() const override {
		return new MeshRenderer(*this);
	};

private:
	// Викликається при активації компонента
	virtual void awake() override;
};
