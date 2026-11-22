#pragma once
#include "Component.h"
#include "NeuralNetwork.h"
#include "AdamOptimizer.h"
#include "Vector3.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <random>

class RigidBody;

// Метод прогнозування траєкторій
enum class TrajectoryMethod
{
	ConstantVelocity,
	Kalman,
	Neural
};

// Сервіс прогнозування траєкторій: стежить за рухомими об'єктами сцени й передбачає їхні положення на кілька секунд уперед
class TrajectoryPredictor : public Component
{
public:
	COMPONENT_TYPE(TrajectoryPredictor)

	// Інтервал між записами положень об'єкта, с
	static constexpr float SAMPLE_INTERVAL = 0.1f;
	// Скільки минулих положень бачить нейромережа
	static const int HISTORY = 10;
	// Скільки майбутніх положень передбачає нейромережа
	static const int FUTURE = 10;
	// Скільки інтервалів запису між сусідніми майбутніми положеннями
	static const int FUTURE_STRIDE = 2;
	// Розширення файлів навчених мереж прогнозу траєкторій
	static const char* FILE_EXTENSION;

	// Знімає себе з ролі активного прогнозувальника сцени
	~TrajectoryPredictor() override;

	// Метод прогнозу для інших компонентів: 0 - стала швидкість, 1 - фільтр Калмана, 2 - нейромережа
	int method = 2;
	// Шлях до файлу навченої мережі прогнозу
	std::string modelAsset = "Assets\\Networks\\Trajectory.traj";
	// Чи навчати мережу на рухомих об'єктах сцени
	bool train = false;
	// Чи продовжувати навчання з файлу, а не з випадкових ваг
	bool continueTraining = false;
	// Стандартне відхилення шуму вимірювання положень, м
	float measurementNoise = 0.0f;
	// Найбільше відхилення шуму, який під час навчання випадково додається до кожного відрізка руху, щоб мережа не ламалася від шуму, м
	float trainingNoise = 0.5f;
	// Стандартне відхилення прискорення в моделі фільтра Калмана, м/с²
	float processNoise = 4.0f;
	// Скільки мілісекунд кадру віддавати навчанню
	float trainMilliseconds = 4.0f;
	// Темп навчання оптимізатора
	float learningRate = 0.001f;
	// Чи малювати прогнозовані траєкторії об'єктів
	bool showPredictions = true;
	// Чи показувати вікно з точністю методів
	bool showWindow = true;

	// Повертає прогнозувальник, що зараз працює в сцені, або nullptr
	static TrajectoryPredictor* getActive();

	// Прогнозує положення об'єкта через time секунд обраним методом; false, якщо об'єкт ще не відстежується
	bool predict(RigidBody* body, float time, Vector3& position);

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	TrajectoryPredictor* instantiate() const override
	{
		return new TrajectoryPredictor(*this);
	}

private:
	// Фільтр Калмана зі сталою швидкістю для однієї осі: положення та швидкість
	struct AxisFilter
	{
		// Оцінка положення
		float position = 0.0f;
		// Оцінка швидкості
		float velocity = 0.0f;
		// Коваріаційна матриця похибки оцінки
		float p00 = 100.0f, p01 = 0.0f, p11 = 100.0f;
	};

	// Відстежуваний об'єкт: історія положень, фільтр Калмана та прогнози
	struct Track
	{
		// Записані положення в площині, від найстарішого
		std::vector<float> history;
		// Останній відомий курс
		float heading = 0.0f;
		// Фільтр Калмана за віссю X
		AxisFilter filterX;
		// Фільтр Калмана за віссю Z
		AxisFilter filterZ;
		// Чи фільтр уже отримав перше вимірювання
		bool filterReady = false;
		// Прогноз нейромережі: зміщення у світових осях для майбутніх моментів
		float neural[FUTURE * 2] = {};
		// Номер запису, для якого обчислено прогноз нейромережі
		long long neuralTick = -1;
		// Чи частина прикладів цього відрізка руху йде на перевірку
		bool validation = false;
		// Висота об'єкта для малювання
		float height = 0.0f;
		// Номер останнього запису, у якому об'єкт бачили
		long long lastSeen = 0;
		// Номер запису, з якого почався поточний безперервний відрізок руху
		long long startTick = 0;
		// Відхилення шуму вимірювань цього відрізка руху
		float noise = 0.0f;
		// Справжнє положення без шуму на останньому записі, щоб перевіряти прогноз за дійсністю
		float trueX = 0.0f, trueZ = 0.0f;
	};

	// Приклад, що чекає на майбутнє: входи мережі та прогнози простих методів на момент створення
	struct PendingSample
	{
		// Об'єкт прикладу
		RigidBody* body;
		// Номер запису, коли приклад створено
		long long tick;
		// Положення в момент створення
		float x, z;
		// Курс у момент створення
		float heading;
		// Входи мережі
		float input[HISTORY * 2 + 1];
		// Прогноз сталої швидкості в системі курсу
		float constant[FUTURE * 2];
		// Прогноз фільтра Калмана в системі курсу
		float kalman[FUTURE * 2];
		// Чи приклад перевірний
		bool validation;
		// Справжні майбутні зміщення в системі курсу, що надходять по одному
		float target[FUTURE * 2];
		// Скільки майбутніх зміщень уже надійшло
		int collected;
	};

	// Готовий приклад: входи, правильні зміщення та прогнози простих методів
	struct Sample
	{
		// Входи мережі
		float input[HISTORY * 2 + 1];
		// Справжні майбутні зміщення в системі курсу
		float target[FUTURE * 2];
		// Прогноз сталої швидкості
		float constant[FUTURE * 2];
		// Прогноз фільтра Калмана
		float kalman[FUTURE * 2];
	};

	// Стає активним прогнозувальником сцени
	void awake() override;
	// Записує положення об'єктів раз на інтервал і готує приклади
	void fixedUpdate() override;
	// Навчає мережу, малює прогнози та вікно
	void update() override;

	// Перечитує мережу з файлу, якщо шлях змінився
	void ensureNetwork();
	// Оновлює фільтр Калмана новим виміряним положенням з відомим відхиленням шуму
	void updateFilter(AxisFilter& filter, float measurement, float noise);
	// Складає входи мережі з історії: минулі положення в системі курсу та швидкість
	bool buildInput(const Track& track, float* input) const;
	// Обчислює прогноз нейромережі для об'єкта на поточний запис
	void updateNeural(Track& track);
	// Переводить зміщення зі світових осей у систему курсу
	static void toHeadingFrame(float dx, float dz, float heading, float& lateral, float& forward);
	// Обчислює точність методів на перевірних прикладах
	void evaluate();
	// Навчає мережу, доки не вичерпано бюджет кадру
	void trainNetwork();
	// Малює вікно з точністю методів
	void drawWindow();

	// Відстежувані об'єкти
	std::unordered_map<RigidBody*, Track> mTracks;
	// Приклади, що чекають на майбутнє
	std::vector<PendingSample> mPending;
	// Навчальні приклади
	std::vector<Sample> mTrainData;
	// Куди записати наступний навчальний приклад, коли сховище заповнене
	size_t mTrainNext = 0;
	// Перевірні приклади
	std::vector<Sample> mValidationData;
	// Куди записати наступний перевірний приклад, коли сховище заповнене
	size_t mValidationNext = 0;
	// Мережа прогнозу
	NeuralNetwork mNetwork;
	// Оптимізатор мережі
	AdamOptimizer mOptimizer;
	// Шлях, з якого прочитано мережу
	std::string mLoadedPath;
	// Чи мережа готова до прогнозу
	bool mNetworkReady = false;
	// Генератор шуму вимірювань і вибору прикладів
	std::mt19937 mRandom{ 7 };
	// Кількість кроків фізики від останнього запису
	int mStepCounter = 0;
	// Номер поточного запису
	long long mTick = 0;
	// Кількість кроків оптимізатора
	long long mBatches = 0;
	// Згладжена похибка навчання
	float mTrainLoss = 0.0f;
	// Середня й кінцева похибки кожного методу з останньої перевірки: стала швидкість, Калман, мережа
	float mAde[3] = {}, mFde[3] = {};
	// Середня похибка мережі за кожну перевірку для графіка
	std::vector<float> mAdeHistory;
	// Найменша середня похибка мережі, з якою її збережено
	float mBestAde = -1.0f;
	// Час останньої перевірки, с
	double mLastEvaluation = 0.0;
	// Чи мережу збережено
	bool mSaved = false;
	// Градієнт пакета
	std::vector<float> mGradients;
};
