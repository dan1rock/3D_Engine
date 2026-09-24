#pragma once
#include "Component.h"
#include "Vector3.h"

// Компонент напрямленого світла, що освітлює сцену паралельними променями вздовж свого напрямку "вперед"
class DirectionalLight : public Component
{
public:
	COMPONENT_TYPE(DirectionalLight)

	DirectionalLight();
	// Деструктор класу DirectionalLight, знімає реєстрацію світла в EntityManager
	~DirectionalLight() override;

	// Встановлює колір світла
	void setColor(float r, float g, float b);

	// Повертає напрямок, у якому світить світло
	Vector3 getDirection();

	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float intensity = 1.0f;

	// Записує колір та яскравість світла у файл сцени
	void serialize(SceneWriter& writer) const override;
	// Відновлює колір та яскравість світла з файлу сцени
	void deserialize(const SceneReader& reader) override;

protected:
	// Конструктор копіювання
	DirectionalLight* instantiate() const override {
		return new DirectionalLight(*this);
	};

	// Реєструє компонент як звичайний компонент і як світло в EntityManager
	void registerComponent() override;

private:
	// Оновлює дані світла у глобальних константах (напрямок, позиція, колір)
	void updateLight();
	void awake() override;

	// Відстань, на яку джерело виноситься проти напрямку світла, щоб промені були паралельними
	float mDistance = 1000.0f;

	friend class EntityManager;
};
