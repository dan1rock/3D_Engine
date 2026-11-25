#pragma once
#include "Component.h"
#include "LearnedDynamics.h"
#include "AdamOptimizer.h"
#include "Vector3.h"
#include <string>
#include <vector>
#include <random>

class Entity;
class Prefab;
class RigidBody;
class Agent;
class ControllableBody;

// Тренер моделі динаміки: копії керованого об'єкта рухаються з випадковими діями та власним мозком, переходи записуються, а мережі навчаються на них
class DynamicsTrainer : public Component
{
public:
	COMPONENT_TYPE(DynamicsTrainer)

	// Повертає симуляції звичайний крок і малювання сцени
	~DynamicsTrainer() override;

	// Образ об'єкта з компонентом, похідним від ControllableBody; агент на ньому вміє ставати в середовище й керувати власним мозком
	Prefab* agentPrefab = nullptr;
	// Середовище, яке отримують агенти образу на початку епізоду, наприклад група маршрутів
	Entity* environment = nullptr;
	// Частка об'єктів, якими керує мозок агента з образу
	float policyShare = 0.25f;
	// Частка об'єктів з випадковими діями, що стартують у середовищі, а не на полі: їхні удари вчать модель зіткнень
	float environmentShare = 0.5f;
	// Кількість об'єктів, що збирають дані
	int bodyCount = 32;
	// Центр відкритого поля, по якому рухається решта об'єктів з випадковими діями
	Vector3 fieldCenter = Vector3(0.0f, 0.5f, 0.0f);
	// Розмір квадратного поля, м
	float fieldSize = 150.0f;
	// Найбільша кількість кроків епізоду одного об'єкта
	int episodeSteps = 1500;
	// Частка епізодів, які йдуть на перевірку, а не на навчання
	float validationShare = 0.15f;

	// Найбільша кількість навчальних переходів; старі витісняються новими
	int datasetCapacity = 300000;
	// Скільки переходів потрібно зібрати перед початком навчання
	int warmupSamples = 20000;
	// Розмір пакета для одного кроку оптимізатора
	int batchSize = 256;
	// Темп навчання оптимізатора Adam
	float learningRate = 0.001f;
	// Скільки мілісекунд кадру віддавати навчанню
	float trainMilliseconds = 8.0f;
	// Горизонт перевірки багатокрокового прогнозу в кроках фізики
	int evaluationHorizon = 25;
	// Чи навчати також модель зіткнень
	bool learnCollisions = true;
	// Кількість променів огляду моделі зіткнень
	int collisionRays = 16;
	// Дальність променів огляду моделі зіткнень, м
	float collisionRange = 20.0f;
	// Горизонт, на якому модель зіткнень передбачає удар, у кроках фізики
	int collisionHorizon = 25;
	// Чи продовжувати навчання моделі з файлу, а не з випадкових ваг
	bool continueTraining = false;
	// Група фільтрації зіткнень об'єктів, щоб вони не зіштовхувалися між собою
	int collisionGroup = 2;
	// Зерно генератора випадкових чисел; 0 - нове зерно щоразу
	int seed = 0;

	// Скільки кроків симуляції виконувати за кадр; 1 - звичайний темп
	int stepsPerFrame = 1;
	// Чи малювати сцену під час збирання даних
	bool renderScene = true;

	// Шлях до файлу, куди зберігається навчена модель
	std::string modelAsset = "Assets\\Networks\\Dynamics.dyn";

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	DynamicsTrainer* instantiate() const override
	{
		return new DynamicsTrainer(*this);
	}

private:
	// Приклад для моделі зіткнень, мітка якого ще не відома: чекає, чи станеться удар протягом горизонту
	struct PendingCollision
	{
		// Запис прикладу: огляд, стан, сума дій від створення, мітка
		std::vector<float> row;
		// Скільки кроків минуло від створення прикладу
		int age = 0;
	};

	// Об'єкт, що збирає дані, та стан його епізоду
	struct Driver
	{
		// Об'єкт
		Entity* entity = nullptr;
		// Керований компонент об'єкта
		ControllableBody* controllable = nullptr;
		// Фізичне тіло
		RigidBody* body = nullptr;
		// Агент образу, або nullptr
		Agent* agent = nullptr;
		// Чи об'єктом керує мозок агента
		bool policy = false;
		// Чи об'єкт з випадковими діями стартує в середовищі
		bool inEnvironment = false;
		// Стан на початку попереднього кроку
		std::vector<float> state;
		// Поза на початку попереднього кроку
		float x = 0.0f, z = 0.0f, heading = 0.0f;
		// Чи є попередній стан, від якого записувати перехід
		bool hasState = false;
		// Кількість кроків поточного епізоду
		int steps = 0;
		// Скільки кроків поспіль об'єкт майже стоїть
		int slowSteps = 0;
		// Номер перевірної траєкторії епізоду, або -1, якщо епізод навчальний
		int trajectory = -1;
		// Поточні випадкові дії
		std::vector<float> actions;
		// Цільові випадкові дії, до яких плавно наближаються поточні
		std::vector<float> targets;
		// Скільки кроків лишилося до вибору нових цільових дій
		int targetTimer = 0;
		// Приклади для моделі зіткнень, що чекають на мітку
		std::vector<PendingCollision> pending;
	};

	// Передає симуляції темп і малювання, навчає мережу в межах бюджету кадру та показує вікно
	void update() override;
	// Записує переходи всіх об'єктів і задає випадкові дії
	void fixedUpdate() override;

	// Створює об'єкти й модель; повертає false, якщо навчання неможливо налаштувати
	bool initialize();
	// Ставить об'єкт на початок нового епізоду
	void respawn(Driver& driver);
	// Додає перехід до навчальних або перевірних даних
	void storeTransition(const Driver& driver, const float* action, const float* next);
	// Додає приклад з відомою міткою до навчальних або перевірних даних моделі зіткнень
	void storeCollision(const Driver& driver, PendingCollision& pending, float label);
	// Оновлює приклади, що чекають на мітку: удар позначає всі, а спливлий горизонт - найстаріші як безпечні
	void updatePending(Driver& driver, const float* action, bool collided);
	// Навчає мережу, доки не вичерпано бюджет кадру
	void train();
	// Обчислює похибку одного кроку та положення на горизонті на перевірних даних і зберігає модель, якщо вона покращилася
	void evaluate();
	// Повертає середню похибку положення на горизонті H на перевірних траєкторіях
	float horizonError(int horizon, int segments);
	// Малює вікно з ходом навчання
	void drawWindow();

	// Модель, що навчається
	LearnedDynamics mModel;
	// Оптимізатор мережі моделі
	AdamOptimizer mOptimizer;
	// Оптимізатор мережі моделі зіткнень
	AdamOptimizer mCollisionOptimizer;
	// Розмір стану
	int mStateSize = 0;
	// Розмір дій
	int mActionSize = 0;
	// Кількість варіантів середовища
	int mVariants = 1;
	// Навчальні приклади моделі зіткнень підряд
	std::vector<float> mCollisionData;
	// Куди записати наступний навчальний приклад зіткнень, коли сховище заповнене
	size_t mCollisionNext = 0;
	// Перевірні приклади моделі зіткнень підряд
	std::vector<float> mCollisionValidation;
	// Куди записати наступний перевірний приклад зіткнень, коли сховище заповнене
	size_t mCollisionValidationNext = 0;
	// Кількість навчальних прикладів з ударом, зібраних від початку
	long long mCollisionPositives = 0;
	// Кількість навчальних прикладів зіткнень, зібраних від початку
	long long mCollisionTotal = 0;
	// Кількість кроків оптимізатора моделі зіткнень
	long long mCollisionBatches = 0;
	// Згладжена похибка навчальних пакетів моделі зіткнень
	float mCollisionTrainLoss = 0.0f;
	// Логістична похибка моделі зіткнень на перевірних даних за кожну перевірку
	std::vector<float> mCollisionLossHistory;
	// Частка вгаданих ударів на перевірних даних з останньої перевірки
	float mCollisionRecall = 0.0f;
	// Частка справжніх ударів серед передбачених з останньої перевірки
	float mCollisionPrecision = 0.0f;
	// Найменша логістична похибка моделі зіткнень, з якою модель збережено
	float mBestCollisionLoss = -1.0f;
	// Об'єкти, що збирають дані
	std::vector<Driver> mDrivers;
	// Навчальні переходи підряд
	std::vector<float> mTrainData;
	// Куди записати наступний навчальний перехід, коли сховище заповнене
	size_t mTrainNext = 0;
	// Перевірні переходи підряд
	std::vector<float> mValidationData;
	// Куди записати наступний перевірний перехід, коли сховище заповнене
	size_t mValidationNext = 0;
	// Перевірні траєкторії для оцінки багатокрокового прогнозу: на кожному кроці стан, дія та поза
	std::vector<std::vector<float>> mTrajectories;
	// Яку перевірну траєкторію замінити наступною, коли їх уже досить
	size_t mTrajectoryNext = 0;
	// Генератор випадкових чисел
	std::mt19937 mRandom;
	// Пакет записів для кроку оптимізатора
	std::vector<const float*> mBatch;

	// Кількість зроблених кроків оптимізатора
	long long mBatches = 0;
	// Згладжена похибка навчальних пакетів
	float mTrainLoss = 0.0f;
	// Похибка одного кроку на перевірних даних за кожну перевірку
	std::vector<float> mValidationHistory;
	// Похибка положення на горизонті за кожну перевірку, м
	std::vector<float> mHorizonHistory;
	// Найменша похибка положення на горизонті, з якою модель збережено
	float mBestHorizonError = -1.0f;
	// Час останньої перевірки, с
	double mLastEvaluation = 0.0;
	// Загальна кількість зібраних переходів
	long long mCollected = 0;
	// Чи навчання вже налаштовано
	bool mInitialized = false;
	// Чи навчання неможливе через помилку налаштування
	bool mFailed = false;
	// Чи модель уже збережено
	bool mSaved = false;
};
