#include "GeneticAlgorithm.h"
#include <algorithm>
#include <numeric>

// Створює популяцію заданого розміру з нульовими векторами параметрів
void GeneticAlgorithm::init(int populationSize, int parameterCount, unsigned int seed)
{
	mPopulation.assign(populationSize, std::vector<float>(parameterCount, 0.0f));
	mFitness.assign(populationSize, 0.0f);
	mBest.assign(parameterCount, 0.0f);
	mBestFitness = 0.0f;
	mMeanFitness = 0.0f;
	mGeneration = 0;
	mRandom.seed(seed);
}

// Повертає розмір популяції
int GeneticAlgorithm::getPopulationSize() const
{
	return (int)mPopulation.size();
}

// Повертає вектор параметрів особини
std::vector<float>& GeneticAlgorithm::getIndividual(int index)
{
	return mPopulation[index];
}

// Задає пристосованість особини за результатами оцінювання
void GeneticAlgorithm::setFitness(int index, float fitness)
{
	mFitness[index] = fitness;
}

// Повертає пристосованість особини
float GeneticAlgorithm::getFitness(int index) const
{
	return mFitness[index];
}

// Створює наступне покоління: еліта переходить без змін, решта - мутанти переможців турнірів з кроком sigma
void GeneticAlgorithm::evolve(int eliteCount, int tournamentSize, float sigma)
{
	int size = (int)mPopulation.size();

	if (size == 0) return;

	std::vector<int> order(size);
	std::iota(order.begin(), order.end(), 0);
	std::stable_sort(order.begin(), order.end(), [this](int a, int b) { return mFitness[a] > mFitness[b]; });

	mBest = mPopulation[order[0]];
	mBestFitness = mFitness[order[0]];
	mMeanFitness = std::accumulate(mFitness.begin(), mFitness.end(), 0.0f) / (float)size;

	eliteCount = std::max(0, std::min(eliteCount, size));

	std::vector<std::vector<float>> next;
	next.reserve(size);

	// Еліта переходить без змін, тож знайдене найкраще рішення не втрачається через невдалу мутацію
	for (int i = 0; i < eliteCount; i++) next.push_back(mPopulation[order[i]]);

	std::normal_distribution<float> normal(0.0f, 1.0f);

	while ((int)next.size() < size)
	{
		std::vector<float> child = mPopulation[tournament(tournamentSize)];

		for (float& value : child) value += sigma * normal(mRandom);

		next.push_back(std::move(child));
	}

	mPopulation = std::move(next);
	std::fill(mFitness.begin(), mFitness.end(), 0.0f);
	mGeneration++;
}

// Повертає номер поточного покоління, починаючи з нуля
int GeneticAlgorithm::getGeneration() const
{
	return mGeneration;
}

// Повертає параметри найкращої особини останнього оціненого покоління
const std::vector<float>& GeneticAlgorithm::getBest() const
{
	return mBest;
}

// Повертає пристосованість найкращої особини останнього оціненого покоління
float GeneticAlgorithm::getBestFitness() const
{
	return mBestFitness;
}

// Повертає середню пристосованість останнього оціненого покоління
float GeneticAlgorithm::getMeanFitness() const
{
	return mMeanFitness;
}

// Повертає генератор випадкових чисел алгоритму, щоб початкову популяцію будували з того самого зерна
std::mt19937& GeneticAlgorithm::getRandom()
{
	return mRandom;
}

// Вибирає батька: найкращого з tournamentSize випадкових особин
int GeneticAlgorithm::tournament(int tournamentSize)
{
	std::uniform_int_distribution<int> pick(0, (int)mPopulation.size() - 1);

	int best = pick(mRandom);

	for (int i = 1; i < tournamentSize; i++)
	{
		int candidate = pick(mRandom);

		if (mFitness[candidate] > mFitness[best]) best = candidate;
	}

	return best;
}
