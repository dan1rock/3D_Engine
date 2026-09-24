#include "DemoComponentTypes.h"
#include "ComponentRegistry.h"
#include "Entity.h"

#include "CarComponent.h"
#include "FollowComponent.h"
#include "FrameCounter.h"
#include "SceneChanger.h"
#include "DemoPlayer.h"
#include "InstantiationTest.h"
#include "ObjectSpawner.h"

// Реєструє компоненти демонстраційного проєкту в редакторі та у файлі сцени
void registerDemoComponentTypes()
{
	ComponentRegistry::registerType("CarComponent", [](Entity* e) -> Component* { return e->addComponent<CarComponent>(); });
	ComponentRegistry::registerType("FollowComponent", [](Entity* e) -> Component* { return e->addComponent<FollowComponent>(); });
	ComponentRegistry::registerType("FrameCounter", [](Entity* e) -> Component* { return e->addComponent<FrameCounter>(); });
	ComponentRegistry::registerType("SceneChanger", [](Entity* e) -> Component* { return e->addComponent<SceneChanger>(); });
	ComponentRegistry::registerType("InstantiationTest", [](Entity* e) -> Component* { return e->addComponent<InstantiationTest>(); });

	// Створювач об'єктів отримує свій образ із файлу сцени, тому тут його ще немає
	ComponentRegistry::registerType("ObjectSpawner", [](Entity* e) -> Component* { return e->addComponent<ObjectSpawner>(nullptr); });

	// Гравець демонстрації створюється з типовими швидкістю та чутливістю огляду
	ComponentRegistry::registerType("DemoPlayer", [](Entity* e) -> Component* { return e->addComponent<DemoPlayer>(2.0f, 0.002f); });
}
