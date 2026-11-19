#pragma once
#include "Component.h"
#include "AgentSensor.h"

class VehicleController;
class RigidBody;

// Причина завершення епізоду агента
enum class EpisodeEnd
{
	None,
	Goal,
	Collision,
	Flipped,
	Stuck,
	Timeout
};

// Мозок агента: на кожному кроці отримує спостереження, обчислює дію для автомобіля та рахує винагороду епізоду
class AgentBrain : public Component
{
public:
	// Розмір вектора дій: газ і кермо
	static const int ACTION_SIZE = 2;

	// Вага просування маршрутом у винагороді
	static constexpr float PROGRESS_WEIGHT = 1.0f;
	// Вага штрафу за зіткнення
	static constexpr float COLLISION_WEIGHT = 5.0f;
	// Вага штрафу за різкі рухи кермом
	static constexpr float STEERING_WEIGHT = 0.05f;
	// Штраф за кожен крок, що підганяє агента
	static constexpr float TIME_WEIGHT = 0.001f;

	// Найбільша кількість кроків в епізоді
	int maxEpisodeSteps = 3000;
	// Швидкість, нижче якої агент вважається таким, що стоїть, м/с
	float stuckSpeed = 0.5f;
	// Скільки секунд агент може стояти, перш ніж епізод завершиться
	float stuckTime = 3.0f;
	// Чи завершувати епізод зіткненням, перевертанням, зупинкою чи лімітом часу; вмикає тренер
	bool episodic = false;

	// Починає новий епізод: обнуляє винагороду, лічильники й ознаку завершення
	void beginEpisode();
	// Перевіряє, чи епізод завершено
	bool isEpisodeDone() const;
	// Повертає причину завершення епізоду
	EpisodeEnd getEpisodeEnd() const;
	// Повертає суму винагород поточного епізоду
	float getEpisodeReward() const;
	// Повертає кількість кроків поточного епізоду
	int getEpisodeSteps() const;
	// Повертає винагороду останнього кроку
	float getLastReward() const;
	// Повертає останній вектор спостережень
	const float* getObservation() const;

	// Повертає назву причини завершення епізоду
	static const char* episodeEndName(EpisodeEnd end);

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Обчислює дію за спостереженням; action отримує газ і кермо в межах [-1; 1]
	virtual void decide(const float* observation, float* action) = 0;

	// Читає сенсори, рахує винагороду переходу та передає автомобілю нову дію
	void fixedUpdate() override;

private:
	// Перевіряє умови завершення епізоду після кроку
	void checkEpisodeEnd(VehicleController* vehicle, AgentSensor* sensor, bool collided);

	// Останній вектор спостережень
	float mObservation[AgentSensor::OBSERVATION_SIZE] = {};
	// Сума винагород епізоду
	float mEpisodeReward = 0.0f;
	// Винагорода останнього кроку
	float mLastReward = 0.0f;
	// Кількість кроків епізоду
	int mEpisodeSteps = 0;
	// Скільки кроків поспіль агент стоїть
	int mStuckSteps = 0;
	// Довжина решти маршруту на попередньому кроці
	float mPreviousRemaining = 0.0f;
	// Дія керма на попередньому кроці
	float mPreviousSteering = 0.0f;
	// Чи вже є попередній крок, від якого рахувати винагороду
	bool mHasPrevious = false;
	// Причина завершення епізоду, None - епізод триває
	EpisodeEnd mEnd = EpisodeEnd::None;
};
