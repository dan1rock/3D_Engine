#pragma once
#include "ModelPredictiveBrain.h"
#include "CarAgent.h"
#include <vector>

class VehicleController;

// Демонстраційний мозок автомобіля з прогнозуючим керуванням: оцінює кандидатів за просуванням маршрутом, коридором дороги, швидкістю в поворотах і перешкодами, а з глухої пози виїжджає окремим маневром
class CarPlannerBrain : public ModelPredictiveBrain
{
public:
	COMPONENT_TYPE(CarPlannerBrain)

	// Задає автомобільні набори дій: чотири рівні газу й дев'ять рівнів керма
	CarPlannerBrain();

	// Колісна база автомобіля для слідування маршрутом у прогнозі, м
	float wheelBase = 2.8f;
	// Відхилення від осі маршруту, яке ще не штрафується, м
	float corridor = 2.5f;
	// Вага штрафу за вихід за межі коридору
	float corridorWeight = 2.0f;
	// Вага слабкого штрафу за відхилення від осі маршруту, що тримає автомобіль ближче до середини дороги
	float centerWeight = 0.02f;
	// Відстань до перешкоди, ближче за яку прогнозований шлях штрафується, м
	float obstacleRadius = 2.0f;
	// Вага штрафу за наближення до перешкод
	float obstacleWeight = 20.0f;
	// Вага штрафу за різкі рухи кермом
	float smoothWeight = 0.02f;
	// Бічне прискорення, яке автомобіль витримує в повороті; з нього й кривини маршруту виходить допустима швидкість, м/с²
	float lateralAcceleration = 5.0f;
	// Сповільнення, з яким автомобіль встигає загальмувати перед поворотом, м/с²
	float braking = 5.0f;
	// Вага штрафу за перевищення допустимої швидкості
	float speedWeight = 1.0f;
	// Кількість власних променів планувальника для пошуку перешкод на дорозі; густіше за сенсор агента, щоб далеку перешкоду не пропустити
	int scanRays = 31;
	// Кут віяла власних променів планувальника, градуси
	float scanAngle = 120.0f;
	// Дальність власних променів планувальника, м
	float scanRange = 40.0f;
	// На скільки секунд продовжувати кінець прогнозованого шляху прямою, щоб помітити перешкоду, до якої шлях веде, с
	float terminalTime = 1.0f;
	// У скільки разів довшим стає горизонт, коли автомобіль майже стоїть: так видно користь від розвороту чи заднього ходу
	int stallHorizonScale = 3;
	// Скільки секунд автомобіль може майже стояти, перш ніж почне виїжджати з глухої пози
	float recoveryDelay = 1.5f;
	// Скільки секунд триває виїзд
	float recoveryTime = 1.2f;

	// Виїжджає з глухої пози, а інакше виконує план прогнозуючого керування
	void decide(Agent& agent, const float* observation, float* action) override;
	// Скидає виїзд і план на початку епізоду
	void onEpisodeBegin(Agent& agent) override;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	CarPlannerBrain* instantiate() const override
	{
		return new CarPlannerBrain(*this);
	}

	// Подовжує горизонт, коли автомобіль майже стоїть
	int getPlanningSteps(const float* state) const override;
	// Майже нерухомий автомобіль тримає дію з набору половину горизонту
	int getHoldSteps(const float* state, int steps) const override;
	// Готує маршрут попереду, допустимі швидкості, перешкоди й прогнози рухомих об'єктів
	void beginPlanning(Agent& agent, const float* state, float x, float z, float heading, int candidates, int steps) override;
	// Після утримання дії з набору веде автомобіль маршрутом методом переслідування точки
	void continueAction(int candidate, int step, const float* state, float x, float z, float heading, float* action) override;
	// Штрафує вихід з коридору, перешкоди, перевищення швидкості й ривки кермом
	float evaluateStep(int candidate, int step, const float* state, const float* action, const float* previousAction, float x, float z, float heading) override;
	// Додає просування маршрутом і штраф за перешкоди на продовженні шляху
	float evaluateEnd(int candidate, const float* state, float x, float z, float heading) override;

private:
	// Готує ламану маршруту попереду агента для оцінки просування
	void prepareRoute(const CarAgent& agent, float x, float z, float heading);
	// Проєктує точку на ламану маршруту серед span відрізків, починаючи з segment; повертає пройдену відстань і відхилення
	float projectOnRoute(float x, float z, int& segment, float& deviation, int span = 3) const;
	// Знаходить точку ламаної маршруту на відстані along від її початку
	void pointOnRoute(float along, float& x, float& z) const;
	// Повертає відстань від точки до найближчого відрізка ламаної маршруту
	float distanceToRoute(float x, float z) const;
	// Обчислює допустиму швидкість у точках ламаної: обмеження кривиною та гальмуванням перед наступними поворотами
	void computeSpeedProfile(float maxSpeed);
	// Повертає допустиму швидкість на відрізку segment на відстані along від початку ламаної
	float allowedSpeed(int segment, float along) const;
	// Повертає штраф за наближення точки до перешкод і рухомих об'єктів у момент time
	float obstaclePenalty(float x, float z, float time) const;
	// Записує прогнозоване положення рухомого об'єкта через time секунд
	void objectAt(int object, float time, float& x, float& z) const;

	// Скільки кроків поспіль автомобіль майже стоїть
	int mStalledSteps = 0;
	// Скільки кроків виїзду лишилося
	int mRecoverySteps = 0;
	// Кермо під час виїзду
	float mRecoverySteering = 0.0f;
	// Газ під час виїзду: назад, якщо позаду більше місця, інакше обережно вперед
	float mRecoveryThrottle = -1.0f;

	// Ламана маршруту попереду: координати точок у площині
	std::vector<float> mRoute;
	// Відстань уздовж маршруту до кожної точки ламаної
	std::vector<float> mRouteDistance;
	// Допустима швидкість у кожній точці ламаної з урахуванням гальмування перед поворотами
	std::vector<float> mRouteSpeed;
	// Точки, де промені планувальника влучили в перешкоди на дорозі
	std::vector<float> mObstacles;
	// Положення найближчих рухомих об'єктів на початку планування
	Vector3 mTrackedPositions[CarAgent::TRACKED_OBJECTS];
	// Швидкості найближчих рухомих об'єктів на початку планування
	Vector3 mTrackedVelocities[CarAgent::TRACKED_OBJECTS];
	// Тіла найближчих рухомих об'єктів
	RigidBody* mTrackedBodies[CarAgent::TRACKED_OBJECTS] = {};
	// Кількість найближчих рухомих об'єктів
	int mTracked = 0;
	// Прогнозовані положення рухомих об'єктів на кожному кроці прогнозу за віссю X
	std::vector<float> mObjectX;
	// Прогнозовані положення рухомих об'єктів на кожному кроці прогнозу за віссю Z
	std::vector<float> mObjectZ;
	// Кількість кроків поточного прогнозу
	int mSteps = 0;
	// Найбільший кут повороту коліс автомобіля
	float mMaxSteering = 0.3f;
	// Пройдена відстань маршрутом на початку прогнозу
	float mStartAlong = 0.0f;
	// Відрізок ламаної, на якому зараз кожен кандидат
	std::vector<int> mSegments;
	// Пройдена кожним кандидатом відстань уздовж ламаної
	std::vector<float> mAlong;
};
