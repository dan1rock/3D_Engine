#pragma once
#include <string>
#include <vector>

class Entity;
class Component;

// Реєстр типів компонентів: дозволяє створювати компонент за його іменем,
// що потрібно і меню редактора, і завантаженню сцени з файлу
class ComponentRegistry
{
public:
	// Функція, яка додає компонент відповідного типу до об'єкта
	typedef Component* (*Creator)(Entity* entity);

	// Реєструє тип компонента під вказаним іменем
	static void registerType(const std::string& name, Creator creator);

	// Створює компонент зареєстрованого типу, або повертає nullptr для невідомого імені
	static Component* create(const std::string& name, Entity* entity);

	// Повертає імена всіх зареєстрованих типів
	static const std::vector<std::string>& getTypeNames();

	// Реєструє типи компонентів, які входять до складу рушія
	static void registerEngineTypes();
};
