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
class VehicleController;
class RigidBody;
class AgentSensor;
class NeuroevolutionBrain;

// Тренер моделі динаміки: автомобілі їздять із випадковими діями та навченою стратегією, переходи записуються, а мережа навчається на них
class DynamicsTrainer : public Component
{
public:
	COMPONENT_TYPE(DynamicsTrainer)

	// Повертає симуляції звичайний крок і малювання сцени
	~DynamicsTrainer() override;

	// Образ автомобіля, з якого створюються водії
	Prefab* agentPrefab = nullptr;
	// Маршрути, якими їздять водії з навченою стратегією: маршрут або група маршрутів
	Entity* route = nullptr;
	// Мережа навченої стратегії для частини водіїв; порожньо - усі їздять випадково
	std::string policyWeights = "Assets\\Networks\\Vehicle.nn";
	// Частка водіїв, що їздять навченою стратегією
	float policyShare = 0.25f;
	// Частка випадкових водіїв, що їздять трасами, а не полем: їхні удари об стіни вчать модель зіткнень
	float randomOnTracks = 0.5f;
	// Кількість водіїв
	int vehicleCount = 32;
	// Центр поля, по якому їздять випадкові водії
	Vector3 fieldCenter = Vector3(0.0f, 0.5f, 0.0f);
	// Розмір квадратного поля для випадкових водіїв, м
	float fieldSize = 150.0f;
	// Найбільша кількість кроків епізоду одного водія
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
	// Чи продовжувати навчання моделі з файлу, а не з випадкових ваг
	bool continueTraining = false;
	// Група фільтрації зіткнень водіїв, щоб вони не зіштовхувалися між собою
	int collisionGroup = 2;
	// Зерно генератора випадкових чисел; 0 - нове зерно щоразу
	int seed = 0;

	// Скільки кроків симуляції виконувати за кадр; 1 - звичайний темп
	int stepsPerFrame = 1;
	// Чи малювати сцену під час збирання даних
	bool renderScene = true;

	// Шлях до файлу, куди зберігається навчена модель
	std::string modelAsset = "Assets\\Networks\\Vehicle Dynamics.dyn";

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	DynamicsTrainer* instantiate() const override
	{
		return new DynamicsTrainer(*this);
	}

private:
	// Точка перевірної траєкторії: стан і поза на початку кроку та дія на ньому
	struct TrajectoryPoint
	{
		// Стан на початку кроку
		float state[4];
		// Дія на кроці
		float action[2];
		// Положення в площині
		float x;
		// Положення в площині
		float z;
		// Курс
		float heading;
	};

	// Приклад для моделі зіткнень, мітка якого ще не відома: чекає, чи станеться удар протягом горизонту
	struct PendingCollision
	{
		// Огляд і стан на момент створення прикладу
		CollisionSample sample;
		// Сума дій від створення прикладу, з якої виходить середня дія
		float actionSum[2] = {};
		// Скільки кроків минуло від створення прикладу
		int age = 0;
	};

	// Водій: автомобіль, що збирає дані, та стан його епізоду
	struct Driver
	{
		// Об'єкт автомобіля
		Entity* entity = nullptr;
		// Керування автомобілем
		VehicleController* vehicle = nullptr;
		// Фізичне тіло корпусу
		RigidBody* body = nullptr;
		// Сенсор маршруту водія з навченою стратегією
		AgentSensor* sensor = nullptr;
		// Мозок водія з навченою стратегією; nullptr - водій їздить випадково
		NeuroevolutionBrain* brain = nullptr;
		// Стан на початку попереднього кроку
		float state[4] = {};
		// Поза на початку попереднього кроку
		float x = 0.0f, z = 0.0f, heading = 0.0f;
		// Чи є попередній стан, від якого записувати перехід
		bool hasState = false;
		// Кількість кроків поточного епізоду
		int steps = 0;
		// Скільки кроків поспіль автомобіль майже стоїть
		int slowSteps = 0;
		// Номер перевірної траєкторії епізоду, або -1, якщо епізод навчальний
		int trajectory = -1;
		// Поточні випадкові газ і кермо
		float throttle = 0.0f, steering = 0.0f;
		// Цільові випадкові газ і кермо, до яких плавно наближаються поточні
		float throttleTarget = 0.0f, steeringTarget = 0.0f;
		// Скільки кроків лишилося до вибору нових цільових дій
		int targetTimer = 0;
		// Чи випадковий водій їздить трасою, а не полем
		bool onTrack = false;
		// Приклади для моделі зіткнень, що чекають на мітку
		std::vector<PendingCollision> pending;
	};

	// Передає симуляції темп і малювання, навчає мережу в межах бюджету кадру та показує вікно
	void update() override;
	// Записує переходи всіх водіїв і задає дії випадковим водіям
	void fixedUpdate() override;

	// Створює водіїв і модель; повертає false, якщо навчання неможливо налаштувати
	bool initialize();
	// Ставить водія на початок нового епізоду
	void respawn(Driver& driver);
	// Вибирає випадковим водієві нові цільові дії
	void pickTargets(Driver& driver);
	// Додає перехід до навчальних або перевірних даних
	void storeTransition(const Driver& driver, const float* action, const float* next);
	// Додає приклад з відомою міткою до навчальних або перевірних даних моделі зіткнень
	void storeCollision(const Driver& driver, const PendingCollision& pending, float label);
	// Оновлює приклади водія, що чекають на мітку: удар позначає всі, а спливлий горизонт - найстаріші як безпечні
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
	// Навчальні приклади моделі зіткнень
	std::vector<CollisionSample> mCollisionData;
	// Куди записати наступний навчальний приклад зіткнень, коли сховище заповнене
	size_t mCollisionNext = 0;
	// Перевірні приклади моделі зіткнень
	std::vector<CollisionSample> mCollisionValidation;
	// Куди записати наступний перевірний приклад зіткнень, коли сховище заповнене
	size_t mCollisionValidationNext = 0;
	// Пакет прикладів зіткнень для кроку оптимізатора
	std::vector<const CollisionSample*> mCollisionBatch;
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
	// Водії
	std::vector<Driver> mDrivers;
	// Маршрути водіїв з навченою стратегією
	std::vector<Entity*> mRoutes;
	// Навчальні переходи
	std::vector<DynamicsTransition> mTrainData;
	// Куди записати наступний навчальний перехід, коли сховище заповнене
	size_t mTrainNext = 0;
	// Перевірні переходи
	std::vector<DynamicsTransition> mValidationData;
	// Куди записати наступний перевірний перехід, коли сховище заповнене
	size_t mValidationNext = 0;
	// Перевірні траєкторії для оцінки багатокрокового прогнозу
	std::vector<std::vector<TrajectoryPoint>> mTrajectories;
	// Яку перевірну траєкторію замінити наступною, коли їх уже досить
	size_t mTrajectoryNext = 0;
	// Генератор випадкових чисел
	std::mt19937 mRandom;
	// Пакет переходів для кроку оптимізатора
	std::vector<const DynamicsTransition*> mBatch;

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
