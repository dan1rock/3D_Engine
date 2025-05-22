#pragma once
#include "RenderComponent.h"

// Компонент для рендеру неба
class SkySphere : public RenderComponent
{
public:
	SkySphere();
	~SkySphere() override;

	// Викликається для рендеру: тимчасово зміщує камеру в центр, рендерить небо, повертає камеру назад
	virtual void render() override;

protected:
	// Конструктор копіювання
	SkySphere* instantiate() const override {
		return new SkySphere(*this);
	};

private:
	// Викликається при активації компонента: створює матеріал, завантажує текстуру та меш, масштабує об'єкт
	void awake() override;
};
