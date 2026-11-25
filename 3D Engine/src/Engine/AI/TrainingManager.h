#pragma once
#include "Component.h"
#include "GeneticAlgorithm.h"
#include <string>
#include <vector>

class Entity;
class Prefab;
class Agent;
class NeuroevolutionBrain;
struct EpisodeSetup;

// Тренер нейроеволюції: створює популяцію агентів з образу, проводить епізоди в навчальному середовищі, перевіряє найкращу особину в окремому й зберігає її мережу
class TrainingManager : public Component
{
public:
	COMPONENT_TYPE(TrainingManager)

	// Повертає симуляції звичайний крок і малювання сцени
	~TrainingManager() override;

	// Образ агента: об'єкт з компонентом, похідним від Agent; мозок NeuroevolutionBrain додається, якщо його немає
	Prefab* agentPrefab = nullptr;
	// Навчальне середовище, яке тренер передає агентам на початку епізоду; що воно означає, вирішує агент
	Entity* environment = nullptr;
	// Перевірне середовище, в якому навчання не відбувається: за ним вибирається мережа, що зберігається
	Entity* validationEnvironment = nullptr;

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
	std::string weightsAsset = "Assets\\Networks\\Agent.nn";

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
		// Варіант навчального середовища
		int variant;
		// Зерно випадкових початкових умов
		unsigned int seed;
	};

	// Агент популяції: об'єкт, його компонент агента й мозок
	struct Member
	{
		// Об'єкт агента
		Entity* entity;
		// Компонент агента
		Agent* agent;
		// Мозок агента
		NeuroevolutionBrain* brain;
	};

	// Передає симуляції темп і малювання та показує вікно з ходом навчання
	void update() override;
	// Керує епізодами: зупиняє агентів, що завершили епізод, і переходить до наступного епізоду чи покоління
	void fixedUpdate() override;

	// Створює популяцію й агентів; повертає false, якщо навчання неможливо налаштувати
	bool initialize();
	// Створює агента з образу та налаштовує його для навчання; false, якщо образ не підходить
	bool createMember(const std::string& name, Member& member);
	// Повертає агента в сцену й починає йому новий епізод
	void beginEpisode(Member& member, const EpisodeSetup& setup);
	// Починає всім агентам популяції поточний епізод
	void startEpisode();
	// Оцінює покоління, зберігає найкращу особину, якщо перевірки немає, і створює наступне покоління
	void finishGeneration();
	// Складає початкові умови епізодів наступного покоління
	void planEpisodes();
	// Веде перевірку найкращої особини в перевірному середовищі й зберігає мережу, що показала себе найкраще
	void updateValidation();
	// Починає перевірному агентові епізод з потрібним варіантом середовища
	void beginValidationEpisode();
	// Повертає крок мутації для поточного покоління
	float currentSigma() const;
	// Малює вікно з ходом навчання
	void drawWindow();

	// Генетичний алгоритм над параметрами мереж
	GeneticAlgorithm mAlgorithm;
	// Агенти популяції
	std::vector<Member> mMembers;
	// Розміри шарів мережі агентів
	std::vector<int> mLayers;
	// Сума винагород кожної особини за епізоди поточного покоління
	std::vector<float> mEpisodeFitness;
	// Початкові умови епізодів поточного покоління
	std::vector<EpisodePlan> mPlans;
	// Кількість варіантів навчального середовища
	int mVariants = 1;
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

	// Агент, що проходить перевірне середовище з найкращою особиною
	Member mValidator = {};
	// Кількість варіантів перевірного середовища
	int mValidationVariants = 0;
	// Параметри особини, яку зараз перевіряють
	std::vector<float> mValidationParameters;
	// Покоління, особину якого зараз перевіряють; -1 - перевірка не йде
	int mValidationGeneration = -1;
	// Останнє покоління, особину якого вже перевірено
	int mValidatedGeneration = -1;
	// Номер варіанта перевірного середовища в поточній перевірці
	int mValidationVariant = 0;
	// Сума оцінок перевірних епізодів у поточній перевірці
	float mValidationScore = 0.0f;
	// Сума винагород перевірних епізодів у поточній перевірці
	float mValidationReward = 0.0f;
	// Середня оцінка кожної перевірки для графіка
	std::vector<float> mValidationHistory;
	// Найкраща середня оцінка перевірки
	float mBestValidation = 0.0f;
	// Чи вже є найкраща перевірка
	bool mHasBestValidation = false;
	// Винагорода перевірки, що дала найкращу оцінку
	float mBestValidationReward = 0.0f;
	// Покоління мережі, що пройшла перевірку найкраще
	int mBestValidationGeneration = 0;
};
