#pragma once
#include "ControllableBody.h"
#include "Vector3.h"

class RigidBody;
class Prefab;
class Transform;

// Демонстраційний автомобіль: підвіска на променях, зчеплення коліс, тяга й кермо за двома нормованими діями - газом і кермом
class VehicleController : public ControllableBody
{
public:
	COMPONENT_TYPE(VehicleController)

	// Створює автомобіль з параметрами за замовчуванням
	VehicleController();
	// Звільняє автомобіль; колеса знищуються разом з дочірніми об'єктами
	~VehicleController();

	// Номер автомобіля у заголовку вікна налагодження
	int id = 0;

	// Жорсткість підвіски
	float force = 1.0f;
	// Коефіцієнт демпфування підвіски
	float damping = 0.1f;
	// Довжина ходу підвіски
	float maxDistance = 1.0f;
	// Максимальна швидкість, за якої тяга зникає
	float maxSpeed = 70.0f;
	// Максимальний кут повороту передніх коліс у радіанах
	float maxSteering = 0.3f;
	// Коефіцієнт бічного зчеплення коліс
	float gripRatio = 0.5f;
	// Максимальна тяга задніх коліс
	float maxThrust = 10.0f;

	// Чи керувати клавішами WASD, коли дій не задає агент
	bool playerInput = true;
	// Чи показувати вікно налагодження з параметрами автомобіля
	bool debugWindow = true;

	// Образ колеса, з якого будуються чотири колеса
	Prefab* wheelPrefab = nullptr;

	// Повертає кількість дій: газ і кермо
	int getActionSize() const override;
	// Задає нормовані дії газу та керма в межах [-1; 1]; відтоді клавіатура автомобілем не керує
	void setActions(const float* actions) override;
	// Записує поточні дії газу та керма
	void getActions(float* actions) const override;
	// Скидає кут керма та дії, наприклад перед новим епізодом
	void resetControl() override;
	// Вимикає під час навчання клавіатуру й вікно налагодження
	void setTrainingMode(bool training) override;
	// Повертає розмір стану: три швидкості та кут повороту коліс
	int getStateSize() const override;
	// Записує стан для моделі динаміки: поздовжня й поперечна швидкості, кутова швидкість рискання, кут повороту коліс
	void getState(float* state) const override;
	// Повертає фізичне тіло корпусу, або nullptr, поки автомобіль не зібрано
	RigidBody* getBody() const override;
	// Вибирає випадкові дії для дослідження руху: здебільшого газ уперед, іноді гальмування чи накат
	void sampleExplorationActions(std::mt19937& random, float* actions) const override;

	// Повертає поточний кут повороту передніх коліс у радіанах
	float getSteering() const;
	// Повертає останню дію газу
	float getThrottleAction() const;
	// Повертає останню дію керма
	float getSteeringAction() const;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	VehicleController* instantiate() const override
	{
		return new VehicleController(*this);
	}

private:
	// Стан одного колеса
	struct wheel
	{
		// Трансформація моделі колеса, що обертається й ходить на підвісці
		Transform* wheelTransform;
		// Точка кріплення колеса до корпусу
		Transform* positionTransform;
		// Чи колесо з правого боку, бо модель лівих коліс розвернуто
		bool rightSide;
		// Кут обертання колеса навколо осі
		float spin;
		// Швидкість обертання колеса за крок
		float spinSpeed;
		// Сила тяги колеса
		float acceleration;
		// Видиме стиснення підвіски
		float springDistance;
	};

	// Скидає ознаку зібраності, щоб колеса побудувалися заново
	void awake() override;
	// Збирає автомобіль: знаходить фізичне тіло й створює колеса
	void start();
	// Малює вікно налагодження
	void update() override;
	// Застосовує дії до коліс і моделює їх на кроці фізики
	void fixedUpdate() override;

	// Моделює підвіску, зчеплення, тягу й опір одного колеса
	void simulateWheel(wheel& wheel);
	// Створює колесо з образу в заданій точці кріплення
	void initWheel(wheel& wheel, Vector3 position);

	// Фізичне тіло корпусу
	RigidBody* mRigidBody = nullptr;

	// Забуває фізичне тіло, якщо його прибрали з машини
	void onComponentRemoved(Component* removed) override;

	// Переднє праве колесо
	wheel mWheelFR = {};
	// Переднє ліве колесо
	wheel mWheelFL = {};
	// Заднє праве колесо
	wheel mWheelBR = {};
	// Заднє ліве колесо
	wheel mWheelBL = {};

	// Поточний кут повороту передніх коліс
	float mSteering = 0.0f;
	// Поточна поздовжня швидкість за модулем
	float mSpeed = 0.0f;

	// Остання дія газу
	float mThrottleAction = 0.0f;
	// Остання дія керма
	float mSteeringAction = 0.0f;
	// Чи дії задає агент, а не клавіатура
	bool mExternalControl = false;

	// Чи автомобіль уже зібрано
	bool isStarted = false;
};
