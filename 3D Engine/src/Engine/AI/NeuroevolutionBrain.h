#pragma once
#include "AgentBrain.h"
#include "NeuralNetwork.h"
#include <string>
#include <vector>

// Мозок агента з нейромережею, ваги якої знайдено нейроеволюцією; навчені ваги читаються з файлу ресурсу
class NeuroevolutionBrain : public AgentBrain
{
public:
	COMPONENT_TYPE(NeuroevolutionBrain)

	// Шлях до файлу навчених ваг у ресурсах проєкту
	std::string weights;

	// Повертає розміри шарів мережі стратегії: спостереження, два приховані шари по 64 нейрони, дії
	static std::vector<int> defaultLayers();

	// Задає параметри мережі напряму, без файлу; так тренер підставляє агенту особину популяції
	void setParameters(const std::vector<float>& parameters);
	// Повертає мережу стратегії
	const NeuralNetwork& getNetwork();

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	NeuroevolutionBrain* instantiate() const override
	{
		return new NeuroevolutionBrain(*this);
	}

	// Обчислює дію прямим проходом мережі
	void decide(const float* observation, float* action) override;

private:
	// Готує мережу: перечитує файл ваг, якщо шлях змінився, або будує нульову мережу
	void ensureNetwork();

	// Мережа стратегії
	NeuralNetwork mNetwork;
	// Шлях, з якого прочитано поточні ваги
	std::string mLoadedPath;
	// Чи мережу вже підготовлено
	bool mReady = false;
	// Чи ваги задає тренер, а не файл
	bool mExternalParameters = false;
};
