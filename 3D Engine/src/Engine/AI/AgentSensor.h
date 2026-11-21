#pragma once
#include "Component.h"
#include "Vector3.h"
#include <vector>

class Entity;
class RaySensor;
class VehicleController;

// Формує вектор спостережень агента-автомобіля: промені, власний рух, наступна точка маршруту й найближчі рухомі об'єкти
class AgentSensor : public Component
{
public:
	COMPONENT_TYPE(AgentSensor)

	// Розмір вектора спостережень
	static const int OBSERVATION_SIZE = 24;
	// Кількість променів, які входять до спостереження
	static const int RAY_COUNT = 9;
	// Кількість найближчих рухомих об'єктів у спостереженні
	static const int TRACKED_OBJECTS = 2;

	// Маршрут: об'єкт, дочірні об'єкти якого по порядку є точками маршруту
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

	// Оновлює просування маршрутом і записує в out OBSERVATION_SIZE нормованих значень
	void observe(float* out);

	// Починає маршрут заново з відрізка, що виходить із точки startWaypoint
	void resetRoute(int startWaypoint);
	// Повертає довжину решти маршруту від агента до цілі, обчислену останнім observe
	float getRemainingDistance() const;
	// Перевіряє, чи агент дістався кінця маршруту
	bool isRouteComplete() const;
	// Повертає номер точки маршруту, до якої агент зараз прямує
	int getTargetWaypoint() const;
	// Повертає точки маршруту, зібрані останнім observe
	const std::vector<Vector3>& getWaypoints() const;
	// Повертає номер відрізка маршруту, на якому зараз агент
	int getSegment() const;
	// Повертає кількість найближчих рухомих об'єктів з останнього observe і записує їхні положення та швидкості
	int getTrackedObjects(Vector3* positions, Vector3* velocities) const;

	// Збирає точки маршруту з дочірніх об'єктів маршруту
	static void collectWaypoints(Entity* route, std::vector<Vector3>& out);
	// Збирає маршрути: сам об'єкт, якщо його дочірні - точки, або кожен його дочірній об'єкт, що має власні точки
	static void collectRoutes(Entity* root, std::vector<Entity*>& out);

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	AgentSensor* instantiate() const override
	{
		return new AgentSensor(*this);
	}

private:
	// Оновлює поточний відрізок маршруту й довжину решти маршруту за положенням агента
	void updateRoute(const Vector3& position);
	// Записує положення двох найближчих рухомих об'єктів зараз і через predictionTime
	void observeObjects(const Vector3& position, const Vector3& velocity, float* out);

	// Точки маршруту, зібрані на останньому кроці
	std::vector<Vector3> mWaypoints;
	// Номер поточного відрізка маршруту: від точки mSegment до наступної
	int mSegment = 0;
	// Довжина решти маршруту
	float mRemaining = 0.0f;
	// Точка маршруту попереду агента, напрям на яку входить у спостереження
	Vector3 mTarget;
	// Чи маршрут пройдено
	bool mComplete = false;
	// Положення найближчих рухомих об'єктів з останнього observe
	Vector3 mTrackedPositions[TRACKED_OBJECTS];
	// Швидкості найближчих рухомих об'єктів з останнього observe
	Vector3 mTrackedVelocities[TRACKED_OBJECTS];
	// Кількість найближчих рухомих об'єктів з останнього observe
	int mTrackedCount = 0;
};
