#include "NeuroevolutionBrain.h"
#include "Agent.h"
#include "Properties.h"
#include <cstdlib>
#include <iostream>

// Повертає розміри шарів мережі: спостереження агента, приховані шари, дії агента
std::vector<int> NeuroevolutionBrain::getLayers(const Agent& agent) const
{
	std::vector<int> layers = { agent.getObservationSize() };

	// Розміри прихованих шарів записано числами через кому чи пробіл
	const char* text = hiddenLayers.c_str();

	while (*text)
	{
		char* end = nullptr;
		long size = std::strtol(text, &end, 10);

		if (end == text)
		{
			text++;
			continue;
		}

		if (size > 0 && size <= 4096) layers.push_back((int)size);

		text = end;
	}

	layers.push_back(agent.getActionSize());

	return layers;
}

// Задає параметри мережі напряму, без файлу; так тренер підставляє агенту особину популяції
void NeuroevolutionBrain::setParameters(const Agent& agent, const std::vector<float>& parameters)
{
	std::vector<int> layers = getLayers(agent);

	if (mNetwork.getLayers() != layers) mNetwork.build(layers);

	mNetwork.setParameters(parameters);
	mExternalParameters = true;
	mReady = true;
}

// Обчислює дію прямим проходом мережі
void NeuroevolutionBrain::decide(Agent& agent, const float* observation, float* action)
{
	ensureNetwork(agent);

	mNetwork.forward(observation, action);
}

// Готує мережу: перечитує файл ваг, якщо шлях змінився, або будує нульову мережу
void NeuroevolutionBrain::ensureNetwork(const Agent& agent)
{
	if (mExternalParameters) return;

	// Мережа перебудовується лише тоді, коли змінився файл, приховані шари чи розміри агента
	if (mReady && mLoadedPath == weights && mLoadedHidden == hiddenLayers && mNetwork.getInputSize() == agent.getObservationSize() && mNetwork.getOutputSize() == agent.getActionSize()) return;

	std::vector<int> layers = getLayers(agent);

	mReady = true;
	mLoadedPath = weights;
	mLoadedHidden = hiddenLayers;

	// Мережа іншого розміру не підходить до спостережень агента, тож її відкидаємо так само, як зіпсований файл
	if (!weights.empty() && mNetwork.load(weights) && mNetwork.getLayers() == layers) return;

	if (!weights.empty()) std::cout << "NeuroevolutionBrain: can't load weights " << weights << " for this agent, agent stays idle" << std::endl;

	// Нульова мережа видає нульові дії: агент без навчених ваг просто стоїть
	mNetwork.build(layers);
}

// Перелічує власні поля для файлу сцени та інспектора
void NeuroevolutionBrain::visitProperties(PropertyVisitor& visitor)
{
	visitor.asset("weights", weights, NeuralNetwork::FILE_EXTENSION);
	visitor.property("hiddenLayers", hiddenLayers);
}
