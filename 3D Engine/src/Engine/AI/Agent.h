#pragma once
#include "Component.h"
#include "Vector3.h"
#include <string>
#include <vector>

class Entity;
class AgentBrain;
class ControllableBody;

// Чим завершився епізод агента
enum class EpisodeEnd
{
	None,
	Success,
	Failure,
	Timeout
};

// Умови нового епізоду, які тренер передає агентові
struct EpisodeSetup
{
	// Середовище епізоду: маршрут, рівень чи група їх; що саме воно означає, вирішує агент
	Entity* environment = nullptr;
	// Номер варіанта середовища, наприклад маршруту з групи
	int variant = 0;
	// Зерно випадкових початкових умов, однакове для всіх агентів популяції, щоб умови були рівні
	unsigned int seed = 0;
	// Чи це перевірний епізод: без випадковостей, щоб результати можна було порівнювати
	bool validation = false;
};

// Базовий клас агента: описує задачу - спостереження, дії, винагороду та епізоди, а рішення приймає мозок AgentBrain
class Agent : public Component
{
public:
	// Найбільша кількість кроків епізоду під час навчання
	int maxEpisodeSteps = 3000;

	// Повертає розмір вектора спостережень
	virtual int getObservationSize() const = 0;
	// Повертає розмір вектора дій; за замовчуванням - скільки дій приймає керований об'єкт агента
	virtual int getActionSize() const;
	// Повертає, скільки різних варіантів епізоду дає середовище; тренер проходить їх по черзі
	virtual int getVariantCount(Entity* environment) const { return 1; }
	// Повертає оцінку завершеного епізоду для перевірки; за замовчуванням - сума винагород
	virtual float getEpisodeScore() const;

	// Починає новий епізод: обнуляє винагороду й лічильники та дає агентові розставити себе
	void beginEpisode(const EpisodeSetup& setup);
	// Завершує епізод з указаним результатом і причиною для журналу
	void endEpisode(EpisodeEnd end, const char* reason = nullptr);
	// Додає винагороду поточного кроку
	void addReward(float reward);

	// Перевіряє, чи епізод завершено
	bool isEpisodeDone() const;
	// Повертає результат завершеного епізоду
	EpisodeEnd getEpisodeEnd() const;
	// Повертає причину завершення епізоду
	const std::string& getEndReason() const;
	// Повертає суму винагород поточного епізоду
	float getEpisodeReward() const;
	// Повертає кількість кроків поточного епізоду
	int getEpisodeSteps() const;
	// Повертає винагороду останнього кроку
	float getLastReward() const;
	// Повертає останній вектор спостережень
	const float* getObservation() const;
	// Повертає останній вектор дій
	const float* getLastAction() const;

	// Вмикає режим навчання: тренер завершує епізоди невдачами й лімітом кроків
	void setTraining(bool training);
	// Перевіряє, чи агент навчається
	bool isTraining() const;
	// Вмикає чи вимикає рішення агента; вимкнений агент не керує тілом, наприклад коли дії задає тренер моделі динаміки
	void setControlEnabled(bool enabled);
	// Повертає мозок агента на тому самому об'єкті або nullptr
	AgentBrain* getBrain();
	// Повертає керований об'єкт агента на тому самому об'єкті або nullptr
	ControllableBody* getControllableBody();

	// Повертає назву результату епізоду
	static const char* episodeEndName(EpisodeEnd end);

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Перевіряє, чи агент готовий діяти; наприклад, автомобіль ще не зібрано
	virtual bool isReady() { return true; }
	// Розставляє агента на початку епізоду за умовами тренера
	virtual void onEpisodeBegin(const EpisodeSetup& setup) {}
	// Записує в observation getObservationSize нормованих значень
	virtual void collectObservations(float* observation) = 0;
	// Після спостережень оцінює наслідки попередньої дії: нараховує винагороду й за потреби завершує епізод
	virtual void onStep() {}
	// Виконує дію; за замовчуванням передає її керованому об'єкту
	virtual void applyActions(const float* action);
	// Обчислює дію, коли мозку немає; false лишає тіло під власним керуванням, наприклад клавіатурою
	virtual bool heuristic(float* action) { return false; }
	// Зупиняє агента після завершення епізоду; за замовчуванням задає нульові дії
	virtual void onEpisodeEnd();

	// Переносить агента в нове положення з нульовими швидкостями й скинутим керуванням
	void teleport(const Vector3& position, const Vector3& rotation);

	// Збирає спостереження, рахує винагороду кроку та виконує рішення мозку
	void fixedUpdate() override;

private:
	// Останній вектор спостережень
	std::vector<float> mObservation;
	// Останній вектор дій
	std::vector<float> mAction;
	// Сума винагород епізоду
	float mEpisodeReward = 0.0f;
	// Винагорода, накопичена на поточному кроці
	float mStepReward = 0.0f;
	// Винагорода останнього кроку
	float mLastReward = 0.0f;
	// Кількість кроків епізоду
	int mEpisodeSteps = 0;
	// Результат епізоду, None - епізод триває
	EpisodeEnd mEnd = EpisodeEnd::None;
	// Причина завершення епізоду
	std::string mEndReason;
	// Чи агент навчається
	bool mTraining = false;
	// Чи агент сам керує тілом
	bool mControlEnabled = true;
};
