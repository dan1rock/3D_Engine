#include "NeuroevolutionBrain.h"
#include "Properties.h"
#include <iostream>

// Повертає розміри шарів мережі стратегії: спостереження, два приховані шари по 64 нейрони, дії
std::vector<int> NeuroevolutionBrain::defaultLayers()
{
	return { AgentSensor::OBSERVATION_SIZE, 64, 64, AgentBrain::ACTION_SIZE };
}

// Задає параметри мережі напряму, без файлу; так тренер підставляє агенту особину популяції
void NeuroevolutionBrain::setParameters(const std::vector<float>& parameters)
{
	if (mNetwork.getLayers() != defaultLayers()) mNetwork.build(defaultLayers());

	mNetwork.setParameters(parameters);
	mExternalParameters = true;
	mReady = true;
}

// Повертає мережу стратегії
const NeuralNetwork& NeuroevolutionBrain::getNetwork()
{
	ensureNetwork();
	return mNetwork;
}

// Обчислює дію прямим проходом мережі
void NeuroevolutionBrain::decide(const float* observation, float* action)
{
	ensureNetwork();

	mNetwork.forward(observation, action);
}

// Готує мережу: перечитує файл ваг, якщо шлях змінився, або будує нульову мережу
void NeuroevolutionBrain::ensureNetwork()
{
	if (mExternalParameters) return;
	if (mReady && mLoadedPath == weights) return;

	mReady = true;
	mLoadedPath = weights;

	// Мережа іншого розміру не підходить до спостережень агента, тож її відкидаємо так само, як зіпсований файл
	if (!weights.empty() && mNetwork.load(weights) && mNetwork.getLayers() == defaultLayers()) return;

	if (!weights.empty()) std::cout << "NeuroevolutionBrain: can't load weights " << weights << ", agent stays idle" << std::endl;

	// Нульова мережа видає нульові дії: агент без навчених ваг просто стоїть
	mNetwork.build(defaultLayers());
}

// Перелічує власні поля для файлу сцени та інспектора
void NeuroevolutionBrain::visitProperties(PropertyVisitor& visitor)
{
	AgentBrain::visitProperties(visitor);

	visitor.asset("weights", weights, NeuralNetwork::FILE_EXTENSION);
}
