#pragma once
#include "Agent.h"
#include "RouteTracker.h"
#include "Vector3.h"

class Entity;
class RigidBody;
class VehicleController;

// Демонстраційний агент-автомобіль: їде маршрутом, бачить перешкоди променями та найближчі рухомі об'єкти й отримує винагороду за просування
class CarAgent : public Agent
{
public:
	COMPONENT_TYPE(CarAgent)

	// Розмір вектора спостережень
	static const int OBSERVATION_SIZE = 24;
	// Кількість променів, які входять до спостереження
	static const int RAY_COUNT = 9;
	// Кількість найближчих рухомих об'єктів у спостереженні
	static const int TRACKED_OBJECTS = 2;

	// Вага просування маршрутом у винагороді
	static constexpr float PROGRESS_WEIGHT = 1.0f;
	// Вага штрафу за зіткнення
	static constexpr float COLLISION_WEIGHT = 5.0f;
	// Вага штрафу за різкі рухи кермом
	static constexpr float STEERING_WEIGHT = 0.05f;
	// Штраф за кожен крок, що підганяє агента
	static constexpr float TIME_WEIGHT = 0.001f;

	// Маршрут: об'єкт, дочірні об'єкти якого по порядку є точками маршруту; тренер замінює його маршрутом із середовища
	Entity* route = nullptr;
	// Дальність, на яку нормуються відстані до цілі й до рухомих об'єктів
	float range = 30.0f;
	// Кутова швидкість рискання, що відповідає одиниці в спостереженні, рад/с
	float maxYawRate = 3.0f;
	// Час, на який прогнозується положення рухомих об'єктів, с
	float predictionTime = 1.0f;
	// Відстань до останньої точки, за якої маршрут вважається пройденим
	float goalRadius = 4.0f;
	// Відстань уздовж маршруту від проєкції агента до точки, напрям на яку він бачить
	float lookahead = 10.0f;
	// Швидкість, нижче якої агент вважається таким, що стоїть, м/с
	float stuckSpeed = 0.5f;
	// Скільки секунд агент може стояти під час навчання, перш ніж епізод завершиться
	float stuckTime = 3.0f;
	// Висота над точкою маршруту, на якій агент з'являється на початку епізоду
	float spawnHeight = 0.5f;
	// Найбільший випадковий зсув старту вбік від маршруту, м
	float startOffset = 1.0f;
	// Найбільше випадкове відхилення початкового курсу від маршруту, градуси
	float startAngle = 10.0f;
	// Найменша частка маршруту, яка має лишатися попереду випадкової точки старту
	float minStartRemaining = 0.5f;

	// Повертає розмір вектора спостережень
	int getObservationSize() const override;
	// Повертає кількість маршрутів у середовищі
	int getVariantCount(Entity* environment) const override;
	// Повертає пройдену частку маршруту у відсотках
	float getEpisodeScore() const override;

	// Повертає стан руху маршрутом з останнього кроку
	const RouteTracker& getRouteTracker() const;
	// Повертає кількість найближчих рухомих об'єктів з останнього кроку і записує їхні положення, швидкості та тіла
	int getTrackedObjects(Vector3* positions, Vector3* velocities, RigidBody** bodies = nullptr) const;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	CarAgent* instantiate() const override
	{
		return new CarAgent(*this);
	}

	// Перевіряє, чи автомобіль уже зібрано, і вмикає облік його зіткнень
	bool isReady() override;
	// Ставить автомобіль на маршрут із середовища: випадкова точка, зсув і курс або початок маршруту для перевірки
	void onEpisodeBegin(const EpisodeSetup& setup) override;
	// Записує промені, власний рух, напрям на маршрут і найближчі рухомі об'єкти
	void collectObservations(float* observation) override;
	// Нараховує винагороду за просування й завершує епізод на цілі, а під час навчання - і на невдачі
	void onStep() override;

private:
	// Записує положення двох найближчих рухомих об'єктів зараз і через predictionTime
	void observeObjects(const Vector3& position, const Vector3& velocity, float* out);

	// Просування маршрутом
	RouteTracker mRoute;
	// Положення найближчих рухомих об'єктів з останнього кроку
	Vector3 mTrackedPositions[TRACKED_OBJECTS];
	// Швидкості найближчих рухомих об'єктів з останнього кроку
	Vector3 mTrackedVelocities[TRACKED_OBJECTS];
	// Тіла найближчих рухомих об'єктів з останнього кроку
	RigidBody* mTrackedBodies[TRACKED_OBJECTS] = {};
	// Кількість найближчих рухомих об'єктів з останнього кроку
	int mTrackedCount = 0;
	// Довжина решти маршруту на попередньому кроці
	float mPreviousRemaining = 0.0f;
	// Дія керма на попередньому кроці
	float mPreviousSteering = 0.0f;
	// Чи вже є попередній крок, від якого рахувати винагороду
	bool mHasPrevious = false;
	// Скільки кроків поспіль агент стоїть
	int mStuckSteps = 0;
};
