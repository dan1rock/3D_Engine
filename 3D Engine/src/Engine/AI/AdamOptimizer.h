#pragma once
#include <vector>

// Оптимізатор Adam: крок градієнтного спуску з адаптивним темпом для кожного параметра
class AdamOptimizer
{
public:
	// Готує оптимізатор для заданої кількості параметрів і обнуляє накопичені моменти
	void init(int parameterCount, float learningRate = 0.001f, float beta1 = 0.9f, float beta2 = 0.999f, float epsilon = 1e-8f);
	// Робить один крок: зменшує параметри в напрямку проти градієнта
	void step(float* parameters, const float* gradients);

	// Повертає кількість зроблених кроків
	int getStepCount() const;

	// Темп навчання
	float learningRate = 0.001f;

private:
	// Коефіцієнт згасання першого моменту
	float mBeta1 = 0.9f;
	// Коефіцієнт згасання другого моменту
	float mBeta2 = 0.999f;
	// Мала добавка, що не дає ділити на нуль
	float mEpsilon = 1e-8f;
	// Кількість зроблених кроків, від якої залежить поправка зміщення моментів
	int mSteps = 0;
	// Ковзне середнє градієнтів
	std::vector<float> mFirst;
	// Ковзне середнє квадратів градієнтів
	std::vector<float> mSecond;
};
