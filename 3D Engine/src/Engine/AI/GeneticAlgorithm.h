#pragma once
#include <vector>
#include <random>

// Генетичний алгоритм над векторами параметрів: елітизм, турнірна селекція та гаусова мутація без схрещування
class GeneticAlgorithm
{
public:
	// Створює популяцію заданого розміру з нульовими векторами параметрів
	void init(int populationSize, int parameterCount, unsigned int seed);

	// Повертає розмір популяції
	int getPopulationSize() const;
	// Повертає вектор параметрів особини
	std::vector<float>& getIndividual(int index);
	// Задає пристосованість особини за результатами оцінювання
	void setFitness(int index, float fitness);
	// Повертає пристосованість особини
	float getFitness(int index) const;

	// Створює наступне покоління: еліта переходить без змін, решта - мутанти переможців турнірів з кроком sigma
	void evolve(int eliteCount, int tournamentSize, float sigma);

	// Повертає номер поточного покоління, починаючи з нуля
	int getGeneration() const;
	// Повертає параметри найкращої особини останнього оціненого покоління
	const std::vector<float>& getBest() const;
	// Повертає пристосованість найкращої особини останнього оціненого покоління
	float getBestFitness() const;
	// Повертає середню пристосованість останнього оціненого покоління
	float getMeanFitness() const;

	// Повертає генератор випадкових чисел алгоритму, щоб початкову популяцію будували з того самого зерна
	std::mt19937& getRandom();

private:
	// Вибирає батька: найкращого з tournamentSize випадкових особин
	int tournament(int tournamentSize);

	// Вектори параметрів усіх особин поточного покоління
	std::vector<std::vector<float>> mPopulation;
	// Пристосованість кожної особини поточного покоління
	std::vector<float> mFitness;
	// Параметри найкращої особини останнього оціненого покоління
	std::vector<float> mBest;
	// Пристосованість найкращої особини останнього оціненого покоління
	float mBestFitness = 0.0f;
	// Середня пристосованість останнього оціненого покоління
	float mMeanFitness = 0.0f;
	// Номер поточного покоління
	int mGeneration = 0;
	// Генератор випадкових чисел для селекції та мутації
	std::mt19937 mRandom;
};
