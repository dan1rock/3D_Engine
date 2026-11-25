#pragma once
#include "NeuralNetwork.h"
#include <string>
#include <vector>
#include <random>

class AdamOptimizer;

// Навчена модель динаміки: нейронна мережа передбачає приріст стану керованого об'єкта в системі координат корпусу, а необов'язкова друга мережа - ймовірність зіткнення
class LearnedDynamics
{
public:
	// Розширення файлів навчених моделей динаміки в ресурсах проєкту
	static const char* FILE_EXTENSION;

	// Створює нову модель для стану й дій заданих розмірів з випадковими вагами й тотожним нормуванням
	void create(int stateSize, int actionSize, std::mt19937& random);
	// Перевіряє, чи модель має мережу, що відповідає її розмірам
	bool isReady() const;
	// Повертає розмір стану
	int getStateSize() const;
	// Повертає розмір дій
	int getActionSize() const;
	// Повертає довжину запису переходу: стан, дія, наступний стан
	int getTransitionSize() const;

	// Обчислює середні й відхилення входів і приростів за записами переходів, що лежать підряд
	void computeNormalization(const std::vector<float>& transitions);
	// Перевіряє, чи нормування вже обчислено
	bool hasNormalization() const;

	// Передбачає наступний стан за поточним станом і дією
	void predict(const float* state, const float* action, float* next) const;
	// Передбачає наступні стани для count пар стан-дія підряд
	void predictBatch(const float* states, const float* actions, int count, float* next) const;

	// Переносить положення й курс у площині на крок за станом, перші три значення якого - поздовжня, поперечна й кутова швидкості; курс 0 означає напрям уздовж осі Z
	static void integratePose(float& x, float& z, float& heading, const float* state, float dt);

	// Робить крок оптимізатора на пакеті записів переходів і повертає середню нормовану похибку пакета
	float trainBatch(const float* const* batch, int count, AdamOptimizer& optimizer);
	// Повертає середню нормовану похибку одного кроку на записах переходів
	float evaluate(const float* const* batch, int count) const;

	// Повертає мережу моделі
	NeuralNetwork& getNetwork();
	// Повертає кількість переходів, на яких модель навчалася
	long long getTrainedSamples() const;
	// Повертає похибку положення на горизонті, виміряну під час останньої перевірки
	float getHorizonError() const;
	// Запам'ятовує результати перевірки, щоб вони потрапили у файл
	void setStatistics(long long trainedSamples, float horizonError, int horizon);

	// Створює нову модель зіткнень з випадковими вагами: rays променів огляду дальністю range, удар передбачається на horizon кроків
	void createCollisionModel(int rays, float range, int horizon, std::mt19937& random);
	// Перевіряє, чи є модель зіткнень
	bool hasCollisionModel() const;
	// Повертає кількість променів огляду моделі зіткнень
	int getCollisionRays() const;
	// Повертає дальність променів огляду моделі зіткнень
	float getCollisionRange() const;
	// Повертає горизонт, на якому модель зіткнень передбачає удар, у кроках фізики
	int getCollisionHorizon() const;
	// Повертає довжину запису прикладу зіткнень: огляд, стан, дія, мітка
	int getCollisionSampleSize() const;
	// Повертає мережу моделі зіткнень
	NeuralNetwork& getCollisionNetwork();
	// Повертає ймовірність зіткнення протягом горизонту, якщо об'єкт з таким оточенням і рухом виконуватиме дію
	float predictCollision(const float* scan, const float* state, const float* action) const;
	// Робить крок оптимізатора моделі зіткнень на пакеті записів і повертає середню логістичну похибку
	float trainCollisionBatch(const float* const* batch, int count, AdamOptimizer& optimizer);
	// Оцінює модель зіткнень: середня логістична похибка, частка вгаданих зіткнень і частка справжніх серед передбачених
	void evaluateCollision(const float* const* batch, int count, float& loss, float& recall, float& precision) const;

	// Записує модель у файл ресурсу
	bool save(const std::string& path) const;
	// Читає модель з файлу ресурсу
	bool load(const std::string& path);

private:
	// Нормує вхід мережі: стан і дію
	void normalizeInput(const float* state, const float* action, float* input) const;
	// Нормує ціль навчання: приріст стану
	void normalizeTarget(const float* state, const float* next, float* target) const;
	// Складає нормований вхід моделі зіткнень
	void collisionInput(const float* scan, const float* state, const float* action, float* input) const;

	// Розмір стану
	int mStateSize = 0;
	// Розмір дій
	int mActionSize = 0;
	// Мережа, що передбачає нормований приріст стану
	NeuralNetwork mNetwork;
	// Середні значення входів мережі
	std::vector<float> mInputMean;
	// Стандартні відхилення входів мережі
	std::vector<float> mInputScale;
	// Середні значення приростів стану
	std::vector<float> mOutputMean;
	// Стандартні відхилення приростів стану
	std::vector<float> mOutputScale;
	// Чи нормування вже обчислено
	bool mNormalized = false;
	// Кількість переходів, на яких модель навчалася
	long long mTrainedSamples = 0;
	// Похибка положення на горизонті з останньої перевірки
	float mHorizonError = 0.0f;
	// Горизонт останньої перевірки в кроках
	int mHorizon = 0;
	// Мережа, що передбачає логарифм шансів зіткнення
	NeuralNetwork mCollisionNetwork;
	// Кількість променів огляду моделі зіткнень
	int mCollisionRays = 0;
	// Дальність променів огляду моделі зіткнень
	float mCollisionRange = 0.0f;
	// Горизонт моделі зіткнень у кроках фізики
	int mCollisionHorizon = 0;
	// Накопичений градієнт пакета
	std::vector<float> mGradients;
	// Робочий вектор входу мережі
	mutable std::vector<float> mInput;
	// Робочий вектор виходу або цілі мережі
	mutable std::vector<float> mOutput;
	// Робочий вектор цілі мережі
	mutable std::vector<float> mTarget;
	// Нормовані входи пакетного прогнозу
	mutable std::vector<float> mBatchInput;
	// Нормовані виходи пакетного прогнозу
	mutable std::vector<float> mBatchOutput;
};
