#pragma once
#include "AgentBrain.h"
#include "NeuralNetwork.h"
#include <string>
#include <vector>

// Мозок з нейромережею, ваги якої знайдено нейроеволюцією; розміри входу й виходу мережі беруться з агента
class NeuroevolutionBrain : public AgentBrain
{
public:
	COMPONENT_TYPE(NeuroevolutionBrain)

	// Шлях до файлу навчених ваг у ресурсах проєкту
	std::string weights;
	// Розміри прихованих шарів мережі через кому
	std::string hiddenLayers = "64, 64";

	// Повертає розміри шарів мережі: спостереження агента, приховані шари, дії агента
	std::vector<int> getLayers(const Agent& agent) const;

	// Задає параметри мережі напряму, без файлу; так тренер підставляє агенту особину популяції
	void setParameters(const Agent& agent, const std::vector<float>& parameters);

	// Обчислює дію прямим проходом мережі
	void decide(Agent& agent, const float* observation, float* action) override;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	NeuroevolutionBrain* instantiate() const override
	{
		return new NeuroevolutionBrain(*this);
	}

private:
	// Готує мережу: перечитує файл ваг, якщо шлях змінився, або будує нульову мережу
	void ensureNetwork(const Agent& agent);

	// Мережа стратегії
	NeuralNetwork mNetwork;
	// Шлях, з якого прочитано поточні ваги
	std::string mLoadedPath;
	// Приховані шари, з якими побудовано поточну мережу
	std::string mLoadedHidden;
	// Чи мережу вже підготовлено
	bool mReady = false;
	// Чи ваги задає тренер, а не файл
	bool mExternalParameters = false;
};
