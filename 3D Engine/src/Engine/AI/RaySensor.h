#pragma once
#include "Component.h"
#include "Vector3.h"
#include <vector>

class RigidBody;

// Променевий сенсор, що імітує лідар: віяло променів у площині корпусу повертає нормовані відстані до перешкод
class RaySensor : public Component
{
public:
	COMPONENT_TYPE(RaySensor)

	// Кількість променів у віялі
	int rayCount = 9;
	// Кут віяла від крайнього лівого до крайнього правого променя в градусах
	float fieldOfView = 180.0f;
	// Дальність сенсора в метрах
	float range = 30.0f;
	// Зсув точки випуску променів уперед від центру об'єкта
	float forwardOffset = 2.0f;
	// Висота точки випуску променів над центром об'єкта
	float heightOffset = 0.3f;

	// Випускає промені й записує в out rayCount відстаней, поділених на дальність: 1 означає, що перешкод немає
	void sense(float* out);
	// Повертає відстані, виміряні останнім викликом sense
	const std::vector<float>& getReadings() const;

	// Повертає точку, з якої випускаються промені
	Vector3 getOrigin();
	// Повертає напрям променя за номером
	Vector3 getRayDirection(int index);

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	RaySensor* instantiate() const override
	{
		return new RaySensor(*this);
	}

private:
	// Відстані останнього вимірювання
	std::vector<float> mReadings;
};
