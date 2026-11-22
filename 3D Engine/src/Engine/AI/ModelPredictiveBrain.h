#pragma once
#include "AgentBrain.h"
#include "Vector3.h"
#include <string>
#include <vector>

class LearnedDynamics;
class VehicleController;
class RaySensor;

// Мозок агента з прогнозуючим керуванням: перебирає поєднання дій із заданих наборів, прогнозує їх навченою моделлю динаміки й виконує найкраще
class ModelPredictiveBrain : public AgentBrain
{
public:
	COMPONENT_TYPE(ModelPredictiveBrain)

	// Шлях до файлу навченої моделі динаміки в ресурсах проєкту
	std::string modelAsset = "Assets\\Networks\\Vehicle Dynamics.dyn";
	// Рівні газу, з яких складаються кандидати, через кому; від'ємний - гальмування й задній хід
	std::string throttlePool = "-1, 0, 0.5, 1";
	// Рівні керма, з яких складаються кандидати, через кому; кожен поєднується з кожним рівнем газу
	std::string steeringPool = "-1, -0.75, -0.5, -0.25, 0, 0.25, 0.5, 0.75, 1";
	// Скільки секунд кандидат тримає дію з набору; далі до кінця горизонту прогноз веде проста стратегія слідування маршрутом
	float holdTime = 0.3f;
	// Колісна база автомобіля для слідування маршрутом у прогнозі, м
	float wheelBase = 2.8f;
	// Довжина послідовності дій у кроках фізики
	int horizon = 25;
	// Через скільки кроків план складається заново; між ними виконується вже складений
	int replanInterval = 2;
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
	// Вага штрафу за ймовірність зіткнення, яку передбачає навчена модель зіткнень
	float collisionWeight = 30.0f;
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
	// Чи малювати вибраний прогнозований шлях
	bool showPlan = true;
	// Чи малювати під вибраним шляхом і решту розглянутих: тонкими лініями, кольором за місцем в оцінці
	bool showCandidates = true;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	ModelPredictiveBrain* instantiate() const override
	{
		return new ModelPredictiveBrain(*this);
	}

	// Виконує дію зі складеного плану, а коли настав час - складає новий
	void decide(const float* observation, float* action) override;

private:
	// Малює вибраний прогнозований шлях
	void update() override;

	// Складає новий план: перебирає поєднання дій із наборів і запам'ятовує найкраще
	void plan(const LearnedDynamics& model, VehicleController* vehicle, AgentSensor* sensor);
	// Готує ламану маршруту попереду агента для оцінки просування
	void prepareRoute(AgentSensor* sensor, float x, float z, float heading);
	// Проєктує точку на ламану маршруту серед span відрізків, починаючи з segment; повертає пройдену відстань і відхилення
	float projectOnRoute(float x, float z, int& segment, float& deviation, int span = 3) const;
	// Знаходить точку ламаної маршруту на відстані along від її початку
	void pointOnRoute(float along, float& x, float& z) const;
	// Повертає дію простої стратегії слідування маршрутом: кермо на точку попереду, газ до допустимої швидкості
	void followRoute(const float* state, float x, float z, float heading, int segment, float along, float maxSteering, float* action) const;
	// Повертає відстань від точки до найближчого відрізка ламаної маршруту
	float distanceToRoute(float x, float z) const;
	// Обчислює допустиму швидкість у точках ламаної: обмеження кривиною та гальмуванням перед наступними поворотами
	void computeSpeedProfile(float maxSpeed);
	// Повертає допустиму швидкість на відрізку segment на відстані along від початку ламаної
	float allowedSpeed(int segment, float along) const;

	// Дії найкращої послідовності: газ і кермо на кожному кроці
	std::vector<float> mPlan;
	// Номер дії плану, яку виконати наступною
	int mPlanStep = 0;
	// Скільки кроків поспіль автомобіль майже стоїть
	int mStalledSteps = 0;
	// Скільки кроків виїзду заднім ходом лишилося
	int mRecoverySteps = 0;
	// Кермо під час виїзду
	float mRecoverySteering = 0.0f;
	// Газ під час виїзду: назад, якщо позаду більше місця, інакше обережно вперед
	float mRecoveryThrottle = -1.0f;
	// Прогнозований шлях найкращої послідовності для малювання
	std::vector<Vector3> mPlannedPath;
	// Прогнозовані шляхи решти кандидатів останнього планування, від найгіршого до найкращого
	std::vector<std::vector<Vector3>> mCandidatePaths;
	// Розібрані рівні газу
	std::vector<float> mThrottleLevels;
	// Розібрані рівні керма
	std::vector<float> mSteeringLevels;
	// Рядок, з якого розібрано рівні газу
	std::string mParsedThrottlePool;
	// Рядок, з якого розібрано рівні керма
	std::string mParsedSteeringPool;

	// Ламана маршруту попереду: координати точок у площині
	std::vector<float> mRoute;
	// Відстань уздовж маршруту до кожної точки ламаної
	std::vector<float> mRouteDistance;
	// Допустима швидкість у кожній точці ламаної з урахуванням гальмування перед поворотами
	std::vector<float> mRouteSpeed;

	// Послідовності дій усіх кандидатів
	std::vector<float> mSequences;
	// Стани кандидатів на поточному кроці прогнозу
	std::vector<float> mStates;
	// Передбачені наступні стани кандидатів
	std::vector<float> mNextStates;
	// Дії кандидатів на поточному кроці прогнозу
	std::vector<float> mActions;
	// Прогнозовані положення всіх кандидатів на всіх кроках
	std::vector<float> mPositions;
};
