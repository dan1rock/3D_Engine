#pragma once
#include "Component.h"
#include <random>

class RigidBody;

// Базовий клас керованого фізичного об'єкта: приймає нормовані дії та віддає свій рух, щоб об'єктом могли керувати агенти та вчитися моделі динаміки
class ControllableBody : public Component
{
public:
	// Повертає кількість дій, якими керується об'єкт
	virtual int getActionSize() const = 0;
	// Задає дії в межах [-1; 1]
	virtual void setActions(const float* actions) = 0;
	// Записує дії, з якими об'єкт рухається зараз
	virtual void getActions(float* actions) const = 0;
	// Скидає внутрішній стан керування, наприклад перед новим епізодом
	virtual void resetControl() {}
	// Перемикає об'єкт у режим навчання: без ручного керування й налагоджувальних вікон
	virtual void setTrainingMode(bool training) {}

	// Повертає розмір динамічного стану; перші три значення стану завжди поздовжня, поперечна швидкості та кутова швидкість рискання
	virtual int getStateSize() const;
	// Записує динамічний стан у системі координат корпусу; похідний клас може дописати після трьох швидкостей власні величини
	virtual void getState(float* state) const;
	// Повертає фізичне тіло об'єкта або nullptr, поки його немає
	virtual RigidBody* getBody() const;
	// Вибирає випадкові дії для дослідження руху: тренер моделі динаміки плавно веде до них керування
	virtual void sampleExplorationActions(std::mt19937& random, float* actions) const;

	// Повертає положення в площині та курс; курс 0 означає напрям уздовж осі Z
	void getPlanarPose(float& x, float& z, float& heading) const;
	// Перевіряє, чи об'єкт не перекинувся: його вісь угору нахилена не більше, ніж дозволяє minUp
	bool isUpright(float minUp = 0.7f) const;
};
