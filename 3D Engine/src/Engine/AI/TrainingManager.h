#pragma once
#include "Component.h"
#include "GeneticAlgorithm.h"
#include "Vector3.h"
#include <string>
#include <vector>

class Entity;
class Prefab;
class NeuroevolutionBrain;

// Тренер агентів: створює популяцію, проводить епізоди на навчальних трасах, перевіряє найкращу особину на окремій трасі й навчає нейроеволюцією
class TrainingManager : public Component
{
public:
	COMPONENT_TYPE(TrainingManager)

	// Повертає симуляції звичайний крок і малювання сцени
	~TrainingManager() override;

	// Образ агента: автомобіль з фізичним тілом; сенсори й мозок додаються, якщо їх немає
	Prefab* agentPrefab = nullptr;
	// Навчальні маршрути: маршрут із точками або група маршрутів, з яких епізоди беруться по черзі
	Entity* route = nullptr;
	// Перевірні маршрути, на яких навчання не відбувається: за ними вибирається мережа, що зберігається
	Entity* validationRoute = nullptr;

	// Розмір популяції, тобто кількість агентів, що навчаються одночасно
	int populationSize = 64;
	// Кількість найкращих особин, що переходять до наступного покоління без змін
	int eliteCount = 4;
	// Розмір турніру для вибору батьків
	int tournamentSize = 3;
	// Початковий крок мутації
	float mutationSigma = 0.05f;
	// Частка початкового кроку мутації, до якої він лінійно зменшується в останньому поколінні
	float finalSigmaRatio = 0.5f;
	// Кількість поколінь навчання
	int generations = 200;
	// Кількість епізодів з різних початкових умов, за якими усереднюється пристосованість
	int episodesPerGeneration = 2;
	// Найбільша кількість кроків в епізоді
	int maxEpisodeSteps = 3000;
	// Висота над точкою маршруту, на якій з'являється агент
	float spawnHeight = 0.5f;
	// Найбільший випадковий зсув старту вбік від маршруту, м
	float startOffset = 1.0f;
	// Найбільше випадкове відхилення початкового курсу від маршруту, градуси
	float startAngle = 10.0f;
	// Найменша частка маршруту, яка має лишатися попереду випадкової точки старту
	float minStartRemaining = 0.5f;
	// Група фільтрації зіткнень агентів, щоб вони не зіштовхувалися між собою
	int collisionGroup = 1;
	// Зерно генератора випадкових чисел; 0 - нове зерно щоразу
	int seed = 0;
	// Чи продовжувати навчання з уже збережених ваг, а не з випадкових
	bool continueTraining = false;

	// Скільки кроків симуляції виконувати за кадр; 1 - звичайний темп
	int stepsPerFrame = 1;
	// Чи малювати сцену під час навчання
	bool renderScene = true;

	// Шлях до файлу, куди зберігаються ваги найкращої мережі
	std::string weightsAsset = "Assets\\Networks\\Vehicle.nn";

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	TrainingManager* instantiate() const override
	{
		return new TrainingManager(*this);
	}

private:
	// Початкові умови епізоду, однакові для всіх агентів популяції
	struct EpisodePlan
	{
		// Маршрут епізоду
		Entity* route;
		// Номер точки маршруту, з якої стартують агенти
		int start;
		// Зсув старту вбік від маршруту, м
		float offset;
		// Відхилення початкового курсу, радіани
		float angle;
	};

	// Передає симуляції темп і малювання та показує вікно з ходом навчання
	void update() override;
	// Керує епізодами: зупиняє агентів, що завершили епізод, і переходить до наступного епізоду чи покоління
	void fixedUpdate() override;

	// Створює популяцію й агентів; повертає false, якщо навчання неможливо налаштувати
	bool initialize();
	// Створює агента з образу та налаштовує його для навчання; nullptr, якщо образ не підходить
	Entity* createAgent(const std::string& name, NeuroevolutionBrain*& brain);
	// Ставить агента на маршрут і починає йому новий епізод
	void placeAgent(Entity* agent, NeuroevolutionBrain* brain, Entity* agentRoute, int start, float offset, float angle);
	// Ставить усіх агентів популяції на старт поточного епізоду
	void startEpisode();
	// Оцінює покоління, зберігає найкращу особину, якщо перевірки немає, і створює наступне покоління
	void finishGeneration();
	// Складає початкові умови епізодів наступного покоління
	void planEpisodes();
	// Веде перевірку найкращої особини на перевірних маршрутах і зберігає мережу, що пройшла їх найкраще
	void updateValidation();
	// Повертає крок мутації для поточного покоління
	float currentSigma() const;
	// Малює вікно з ходом навчання
	void drawWindow();

	// Генетичний алгоритм над параметрами мереж
	GeneticAlgorithm mAlgorithm;
	// Агенти популяції
	std::vector<Entity*> mAgents;
	// Мозки агентів у тому самому порядку
	std::vector<NeuroevolutionBrain*> mBrains;
	// Сума винагород кожної особини за епізоди поточного покоління
	std::vector<float> mEpisodeFitness;
	// Початкові умови епізодів поточного покоління
	std::vector<EpisodePlan> mPlans;
	// Навчальні маршрути
	std::vector<Entity*> mRoutes;
	// Найкраща пристосованість кожного покоління для графіка
	std::vector<float> mBestHistory;
	// Середня пристосованість кожного покоління для графіка
	std::vector<float> mMeanHistory;
	// Номер епізоду в поточному поколінні
	int mEpisode = 0;
	// Кількість кроків поточного епізоду
	int mEpisodeStep = 0;
	// Загальна кількість кроків симуляції від початку навчання
	long long mTotalSteps = 0;
	// Чи навчання вже налаштовано
	bool mInitialized = false;
	// Чи навчання неможливе через помилку налаштування
	bool mFailed = false;
	// Чи задану кількість поколінь пройдено; далі агенти лише показують найкращу особину
	bool mFinished = false;
	// Чи ваги найкращої мережі вдалося зберегти останнього разу
	bool mSaved = false;

	// Агент, що проходить перевірні маршрути з найкращою особиною
	Entity* mValidator = nullptr;
	// Мозок перевірного агента
	NeuroevolutionBrain* mValidatorBrain = nullptr;
	// Перевірні маршрути
	std::vector<Entity*> mValidationRoutes;
	// Параметри особини, яку зараз перевіряють
	std::vector<float> mValidationParameters;
	// Покоління, особину якого зараз перевіряють; -1 - перевірка не йде
	int mValidationGeneration = -1;
	// Останнє покоління, особину якого вже перевірено
	int mValidatedGeneration = -1;
	// Номер перевірного маршруту в поточній перевірці
	int mValidationRouteIndex = 0;
	// Сума пройдених часток перевірних маршрутів у поточній перевірці
	float mValidationCompletion = 0.0f;
	// Сума винагород перевірних маршрутів у поточній перевірці
	float mValidationReward = 0.0f;
	// Середня пройдена частка перевірних маршрутів кожної перевірки, у відсотках
	std::vector<float> mValidationHistory;
	// Найкраща середня пройдена частка перевірних маршрутів
	float mBestValidation = -1.0f;
	// Винагорода перевірки, що дала найкращу частку
	float mBestValidationReward = 0.0f;
	// Покоління мережі, що пройшла перевірку найкраще
	int mBestValidationGeneration = 0;
};
