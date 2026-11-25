#pragma once
#include "Component.h"

class Agent;

// Базовий клас мозку агента: за спостереженням обчислює дію; від нього походять нейромережевий мозок і прогнозуюче керування
class AgentBrain : public Component
{
public:
	// Обчислює дію агента за спостереженням; action має getActionSize агента значень у межах [-1; 1]
	virtual void decide(Agent& agent, const float* observation, float* action) = 0;
	// Скидає внутрішній стан мозку на початку епізоду
	virtual void onEpisodeBegin(Agent& agent) {}
};
