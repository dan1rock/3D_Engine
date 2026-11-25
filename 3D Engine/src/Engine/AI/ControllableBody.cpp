#include "ControllableBody.h"
#include "RigidBody.h"
#include "Entity.h"
#include <cmath>

// Повертає розмір динамічного стану; перші три значення стану завжди поздовжня, поперечна швидкості та кутова швидкість рискання
int ControllableBody::getStateSize() const
{
	return 3;
}

// Записує динамічний стан у системі координат корпусу; похідний клас може дописати після трьох швидкостей власні величини
void ControllableBody::getState(float* state) const
{
	RigidBody* body = getBody();

	if (!body)
	{
		state[0] = state[1] = state[2] = 0.0f;
		return;
	}

	Vector3 position, right, up, forward;
	body->getPose(position, right, up, forward);

	Vector3 velocity = body->getVelocity();
	Vector3 angular = body->getAngularVelocity();

	state[0] = velocity * forward;
	state[1] = velocity * right;
	state[2] = angular * up;
}

// Повертає фізичне тіло об'єкта або nullptr, поки його немає
RigidBody* ControllableBody::getBody() const
{
	return mOwner ? mOwner->getComponent<RigidBody>() : nullptr;
}

// Вибирає випадкові дії для дослідження руху: тренер моделі динаміки плавно веде до них керування
void ControllableBody::sampleExplorationActions(std::mt19937& random, float* actions) const
{
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);

	// Частина дій нульова, щоб модель бачила й рух без керування
	for (int i = 0; i < getActionSize(); i++)
	{
		actions[i] = unit(random) < 0.3f ? 0.0f : unit(random) * 2.0f - 1.0f;
	}
}

// Повертає положення в площині та курс; курс 0 означає напрям уздовж осі Z
void ControllableBody::getPlanarPose(float& x, float& z, float& heading) const
{
	Vector3 position, right, up, forward;
	RigidBody* body = getBody();

	// Поза фізичного тіла свіжіша за трансформацію, яку синхронізують лише раз на крок
	if (body) body->getPose(position, right, up, forward);
	else
	{
		position = mOwner->getTransform()->getPosition();
		forward = mOwner->getTransform()->getForward();
	}

	x = position.x;
	z = position.z;
	heading = std::atan2(forward.x, forward.z);
}

// Перевіряє, чи об'єкт не перекинувся: його вісь угору нахилена не більше, ніж дозволяє minUp
bool ControllableBody::isUpright(float minUp) const
{
	RigidBody* body = getBody();

	if (!body) return true;

	Vector3 position, right, up, forward;
	body->getPose(position, right, up, forward);

	return up.y > minUp;
}
