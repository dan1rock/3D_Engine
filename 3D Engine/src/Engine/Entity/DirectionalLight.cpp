#include "DirectionalLight.h"
#include <cmath>
#include "Entity.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "EntityManager.h"
#include "Properties.h"

DirectionalLight::DirectionalLight()
{
}

// Деструктор класу DirectionalLight, знімає реєстрацію світла в EntityManager
DirectionalLight::~DirectionalLight()
{
	EntityManager::get()->unregisterLight(this);
}

// Встановлює колір світла
void DirectionalLight::setColor(float r, float g, float b)
{
	color[0] = r;
	color[1] = g;
	color[2] = b;
}

// Повертає напрямок, у якому світить світло
Vector3 DirectionalLight::getDirection()
{
	return mOwner->getTransform()->getForward();
}

// Реєструє компонент як звичайний компонент і як світло в EntityManager
void DirectionalLight::registerComponent()
{
	Component::registerComponent();
	EntityManager::get()->registerLight(this);
}

// Оновлює дані світла у глобальних константах (напрямок, позиція, колір)
void DirectionalLight::updateLight()
{
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	Vector3 direction = getDirection();

	constantData->lightDir[0] = direction.x;
	constantData->lightDir[1] = direction.y;
	constantData->lightDir[2] = direction.z;

	// Виносить джерело далеко проти напрямку світла, щоб освітлення сцени було майже паралельним
	Vector3 position = -direction * mDistance;

	constantData->lightPos[0] = position.x;
	constantData->lightPos[1] = position.y;
	constantData->lightPos[2] = position.z;

	// Колір задано в sRGB, а освітлення рахується в лінійному просторі
	for (int i = 0; i < 3; i++)
	{
		float c = color[i];
		float linear = c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);

		constantData->lightColor[i] = linear * intensity;
	}

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}

void DirectionalLight::awake()
{
}
// Перелічує власні поля для файлу сцени та інспектора
void DirectionalLight::visitProperties(PropertyVisitor& visitor)
{
	visitor.color("lightColor", color, 3);
	visitor.property("intensity", intensity, 0.05f);
}
