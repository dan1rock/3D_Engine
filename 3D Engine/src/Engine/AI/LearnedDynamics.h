#pragma once
#include "NeuralNetwork.h"
#include <string>
#include <vector>
#include <random>

class AdamOptimizer;

// Перехід стану транспортного засобу за один крок фізики під дією керування
struct DynamicsTransition
{
	// Стан до кроку: поздовжня й поперечна швидкості, кутова швидкість рискання, кут повороту коліс
	float state[4];
	// Дія на кроці: газ і кермо
	float action[2];
	// Стан після кроку
	float next[4];
};

// Навчена модель динаміки: нейронна мережа передбачає приріст стану транспортного засобу в системі координат корпусу
class LearnedDynamics
{
public:
	// Розмір вектора стану
	static const int STATE_SIZE = 4;
	// Розмір вектора дій
	static const int ACTION_SIZE = 2;
	// Розширення файлів навчених моделей динаміки в ресурсах проєкту
	static const char* FILE_EXTENSION;

	// Розміри шарів мережі: стан і дія, два приховані шари по 128 нейронів, приріст стану
	static std::vector<int> defaultLayers();

	// Створює нову модель з випадковими вагами й тотожним нормуванням
	void create(std::mt19937& random);
	// Перевіряє, чи модель має мережу потрібного розміру
	bool isReady() const;

	// Обчислює середні й відхилення входів і приростів за вибіркою переходів
	void computeNormalization(const std::vector<DynamicsTransition>& data);
	// Перевіряє, чи нормування вже обчислено
	bool hasNormalization() const;

	// Передбачає наступний стан за поточним станом і дією
	void predict(const float* state, const float* action, float* next) const;
	// Передбачає наступні стани для count пар стан-дія підряд
	void predictBatch(const float* states, const float* actions, int count, float* next) const;

	// Переносить положення й курс у площині на крок за передбаченим станом; курс 0 означає напрям уздовж осі Z
	static void integratePose(float& x, float& z, float& heading, const float* state, float dt);

	// Робить крок оптимізатора на пакеті переходів і повертає середню нормовану похибку пакета
	float trainBatch(const DynamicsTransition* const* batch, int count, AdamOptimizer& optimizer);
	// Повертає середню нормовану похибку одного кроку на переходах
	float evaluate(const DynamicsTransition* const* batch, int count) const;

	// Повертає мережу моделі
	NeuralNetwork& getNetwork();
	// Повертає кількість переходів, на яких модель навчалася
	long long getTrainedSamples() const;
	// Повертає похибку положення на горизонті, виміряну під час останньої перевірки
	float getHorizonError() const;
	// Запам'ятовує результати перевірки, щоб вони потрапили у файл
	void setStatistics(long long trainedSamples, float horizonError, int horizon);

	// Записує модель у файл ресурсу
	bool save(const std::string& path) const;
	// Читає модель з файлу ресурсу
	bool load(const std::string& path);

private:
	// Нормує вхід мережі: стан і дію
	void normalizeInput(const float* state, const float* action, float* input) const;
	// Нормує ціль навчання: приріст стану
	void normalizeTarget(const float* state, const float* next, float* target) const;

	// Мережа, що передбачає нормований приріст стану
	NeuralNetwork mNetwork;
	// Середні значення входів мережі
	float mInputMean[6] = {};
	// Стандартні відхилення входів мережі
	float mInputScale[6] = { 1, 1, 1, 1, 1, 1 };
	// Середні значення приростів стану
	float mOutputMean[4] = {};
	// Стандартні відхилення приростів стану
	float mOutputScale[4] = { 1, 1, 1, 1 };
	// Чи нормування вже обчислено
	bool mNormalized = false;
	// Кількість переходів, на яких модель навчалася
	long long mTrainedSamples = 0;
	// Похибка положення на горизонті з останньої перевірки
	float mHorizonError = 0.0f;
	// Горизонт останньої перевірки в кроках
	int mHorizon = 0;
	// Накопичений градієнт пакета
	std::vector<float> mGradients;
	// Нормовані входи пакетного прогнозу
	mutable std::vector<float> mBatchInput;
	// Нормовані виходи пакетного прогнозу
	mutable std::vector<float> mBatchOutput;
};
