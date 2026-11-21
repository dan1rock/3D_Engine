#include "AdamOptimizer.h"
#include <cmath>

// Готує оптимізатор для заданої кількості параметрів і обнуляє накопичені моменти
void AdamOptimizer::init(int parameterCount, float rate, float beta1, float beta2, float epsilon)
{
	learningRate = rate;
	mBeta1 = beta1;
	mBeta2 = beta2;
	mEpsilon = epsilon;
	mSteps = 0;
	mFirst.assign(parameterCount, 0.0f);
	mSecond.assign(parameterCount, 0.0f);
}

// Робить один крок: зменшує параметри в напрямку проти градієнта
void AdamOptimizer::step(float* parameters, const float* gradients)
{
	mSteps++;

	// Поправка зміщення: на перших кроках моменти ще не накопичилися й занижені
	float firstCorrection = 1.0f - std::pow(mBeta1, (float)mSteps);
	float secondCorrection = 1.0f - std::pow(mBeta2, (float)mSteps);
	float rate = learningRate * std::sqrt(secondCorrection) / firstCorrection;

	size_t count = mFirst.size();

	for (size_t i = 0; i < count; i++)
	{
		float g = gradients[i];

		mFirst[i] = mBeta1 * mFirst[i] + (1.0f - mBeta1) * g;
		mSecond[i] = mBeta2 * mSecond[i] + (1.0f - mBeta2) * g * g;

		parameters[i] -= rate * mFirst[i] / (std::sqrt(mSecond[i]) + mEpsilon);
	}
}

// Повертає кількість зроблених кроків
int AdamOptimizer::getStepCount() const
{
	return mSteps;
}
