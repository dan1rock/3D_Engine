#include "DirectionalLight.h"
#include "Entity.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "EntityManager.h"
#include "SceneIO.h"

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

	constantData->lightColor[0] = color[0] * intensity;
	constantData->lightColor[1] = color[1] * intensity;
	constantData->lightColor[2] = color[2] * intensity;

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}

void DirectionalLight::awake()
{
}

// Записує колір та яскравість світла у файл сцени
void DirectionalLight::serialize(SceneWriter& writer) const
{
	writer.write("lightcolor", Vector3(color[0], color[1], color[2]));
	writer.write("intensity", intensity);
}

// Відновлює колір та яскравість світла з файлу сцени
void DirectionalLight::deserialize(const SceneReader& reader)
{
	Vector3 loaded = reader.read("lightcolor", Vector3(color[0], color[1], color[2]));

	color[0] = loaded.x;
	color[1] = loaded.y;
	color[2] = loaded.z;

	intensity = reader.read("intensity", intensity);
}
