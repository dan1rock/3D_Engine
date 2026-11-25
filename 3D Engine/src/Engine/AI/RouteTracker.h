#pragma once
#include "Vector3.h"
#include <vector>

class Entity;

// Слідкує за просуванням об'єкта маршрутом: маршрут - об'єкт, дочірні об'єкти якого по порядку є точками; рух рахується в горизонтальній площині
class RouteTracker
{
public:
	// Відстань уздовж маршруту від проєкції об'єкта до точки попереду, на яку він має прямувати
	float lookahead = 10.0f;
	// Відстань до останньої точки, за якої маршрут вважається пройденим
	float goalRadius = 4.0f;

	// Задає маршрут і починає його з відрізка, що виходить із точки startWaypoint
	void setRoute(Entity* route, int startWaypoint = 0);
	// Повертає поточний маршрут
	Entity* getRoute() const;
	// Оновлює просування за положенням об'єкта; точки перечитуються, бо їх можуть пересунути в редакторі
	void update(const Vector3& position);

	// Повертає точки маршруту з останнього оновлення
	const std::vector<Vector3>& getWaypoints() const;
	// Повертає номер відрізка маршруту, на якому зараз об'єкт
	int getSegment() const;
	// Повертає довжину решти маршруту від об'єкта до кінця
	float getRemainingDistance() const;
	// Повертає повну довжину маршруту
	float getLength() const;
	// Повертає частку пройденого маршруту від 0 до 1
	float getCompletion() const;
	// Повертає точку маршруту на відстані lookahead попереду об'єкта
	const Vector3& getTarget() const;
	// Перевіряє, чи об'єкт дістався кінця маршруту
	bool isComplete() const;

	// Збирає точки маршруту з дочірніх об'єктів маршруту
	static void collectWaypoints(Entity* route, std::vector<Vector3>& out);
	// Збирає маршрути: сам об'єкт, якщо його дочірні - точки, або кожен його увімкнений дочірній об'єкт, що має власні точки
	static void collectRoutes(Entity* root, std::vector<Entity*>& out);
	// Повертає довжину ламаної в горизонтальній площині від точки start до кінця
	static float pathLength(const std::vector<Vector3>& waypoints, int start = 0);

private:
	// Маршрут
	Entity* mRoute = nullptr;
	// Точки маршруту з останнього оновлення
	std::vector<Vector3> mWaypoints;
	// Номер поточного відрізка маршруту: від точки mSegment до наступної
	int mSegment = 0;
	// Довжина решти маршруту
	float mRemaining = 0.0f;
	// Точка маршруту попереду об'єкта
	Vector3 mTarget;
	// Чи маршрут пройдено
	bool mComplete = false;
};
