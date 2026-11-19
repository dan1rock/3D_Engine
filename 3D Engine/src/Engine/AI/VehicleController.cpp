#include "VehicleController.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Prefab.h"
#include "Input.h"
#include "EngineTime.h"
#include "imgui.h"
#include <iostream>
#include <algorithm>

#include <string>
#include "Properties.h"

// Створює автомобіль з параметрами за замовчуванням
VehicleController::VehicleController()
{
}

// Звільняє автомобіль; колеса знищуються разом з дочірніми об'єктами
VehicleController::~VehicleController()
{
}

// Скидає ознаку зібраності, щоб колеса побудувалися заново
void VehicleController::awake()
{
	isStarted = false;
}

// Задає нормовані дії газу та керма в межах [-1; 1]; відтоді клавіатура автомобілем не керує
void VehicleController::setActions(float throttle, float steering)
{
	mThrottleAction = std::max(-1.0f, std::min(throttle, 1.0f));
	mSteeringAction = std::max(-1.0f, std::min(steering, 1.0f));
	mExternalControl = true;
}

// Скидає кут керма та дії, наприклад перед новим епізодом
void VehicleController::resetState()
{
	mSteering = 0.0f;
	mSpeed = 0.0f;
	mThrottleAction = 0.0f;
	mSteeringAction = 0.0f;
}

// Повертає поточний кут повороту передніх коліс у радіанах
float VehicleController::getSteering() const
{
	return mSteering;
}

// Повертає останню дію газу
float VehicleController::getThrottleAction() const
{
	return mThrottleAction;
}

// Повертає останню дію керма
float VehicleController::getSteeringAction() const
{
	return mSteeringAction;
}

// Повертає фізичне тіло корпусу, або nullptr, поки автомобіль не зібрано
RigidBody* VehicleController::getRigidBody() const
{
	return mRigidBody;
}

// Забуває фізичне тіло, якщо його прибрали з машини: далі fixedUpdate просто нічого не робить
void VehicleController::onComponentRemoved(Component* removed)
{
	if (removed == mRigidBody) mRigidBody = nullptr;
}

// Збирає автомобіль: знаходить фізичне тіло й створює колеса
void VehicleController::start()
{
	isStarted = true;

	// Без образу колеса чи фізичного тіла машина просто не вмикається, замість того щоб зламати гру
	if (wheelPrefab == nullptr || getOwner()->getComponent<RigidBody>() == nullptr)
	{
		std::cout << "VehicleController: no wheel prefab or rigid body, car stays disabled" << std::endl;
		return;
	}

	mRigidBody = getOwner()->getComponent<RigidBody>();
	mRigidBody->setCenterOfMass(Vector3(0.0f, 0.3f, -0.1f));

	Transform* transform = mOwner->getTransform();
	Vector3 basePos = Vector3(0, 1, 0) * 0.1f;
	float base = 0.85f;

	std::list<Entity*> children = *mOwner->getChildren();

	for (auto* child : children) {
		child->destroy();
	}

	initWheel(mWheelFR, basePos + Vector3(1, 0, 0) * base + Vector3(0, 0, 1) * base * 1.53f);
	initWheel(mWheelFL, basePos - Vector3(1, 0, 0) * base + Vector3(0, 0, 1) * base * 1.53f);
	initWheel(mWheelBR, basePos + Vector3(1, 0, 0) * base - Vector3(0, 0, 1) * base * 1.8f);
	initWheel(mWheelBL, basePos - Vector3(1, 0, 0) * base - Vector3(0, 0, 1) * base * 1.8f);

	mWheelFR.rightSide = true;
	mWheelBR.rightSide = true;
}

// Малює вікно налагодження
void VehicleController::update()
{
	if (!isStarted)
	{
		start();
	}

	if (!debugWindow) return;

	std::string name = "Car " + std::to_string(id);
	const char* cname = name.c_str();

	ImGui::Begin(cname, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
	ImGui::SliderFloat("Speed", &mSpeed, 0.0f, 100.0f);
	ImGui::SliderFloat("MaxSpeed", &maxSpeed, 0.0f, 200.0f);
	ImGui::SliderFloat("MaxSteering", &maxSteering, 0.0f, 1.0f);
	ImGui::SliderFloat("SpringForce", &force, 0.0f, 100.0f);
	ImGui::SliderFloat("SpringDamping", &damping, 0.0f, 50.0f);
	ImGui::SliderFloat("SpringDistance", &maxDistance, 0.0f, 2.0f);
	ImGui::SliderFloat("Grip", &gripRatio, 0.0f, 2.0f);
	if (ImGui::Button("Copy")) {
		Entity* newCar = mOwner->instantiate();
		newCar->getComponent<VehicleController>()->id = id + 1;
		newCar->getTransform()->setPosition(mOwner->getTransform()->getPosition() + mOwner->getTransform()->getRight() * 3.0f);
	};
	ImGui::End();
}

// Застосовує дії до коліс і моделює їх на кроці фізики
void VehicleController::fixedUpdate()
{
	// Автомобіль, створений посеред кадру, збирається вже на першому кроці фізики, а не чекає оновлення кадру
	if (!isStarted)
	{
		start();
	}

	if (!mRigidBody) return;

	float carSpeed = mRigidBody->getVelocity() * mOwner->getTransform()->getForward();
	float absoluteSpeed = carSpeed;
	if (absoluteSpeed < 0.0f) absoluteSpeed = -absoluteSpeed;
	mSpeed = absoluteSpeed;

	float accelerationRatio = 1.0f - absoluteSpeed / maxSpeed;

	// Клавіатура формує ті самі нормовані дії, що й агент
	if (!mExternalControl && playerInput)
	{
		mThrottleAction = 0.0f;
		mSteeringAction = 0.0f;

		if (Input::getKey('A')) mSteeringAction -= 1.0f;
		if (Input::getKey('D')) mSteeringAction += 1.0f;
		if (Input::getKey('W')) mThrottleAction += 1.0f;
		if (Input::getKey('S')) mThrottleAction -= 1.0f;
	}

	float targetSteering = mSteeringAction * maxSteering;

	mSteering += (targetSteering - mSteering) * 0.1f;

	float acceleration = mThrottleAction * maxThrust;

	if (carSpeed * acceleration >= 0.0f)
	{
		acceleration *= accelerationRatio;
	}

	mWheelFR.positionTransform->setLocalRotation(Vector3(0, mSteering, 0));
	mWheelFL.positionTransform->setLocalRotation(Vector3(0, mSteering, 0));

	mWheelBL.acceleration = acceleration;
	mWheelBR.acceleration = acceleration;

	simulateWheel(mWheelFR);
	simulateWheel(mWheelFL);
	simulateWheel(mWheelBL);
	simulateWheel(mWheelBR);

	mRigidBody->addForce(Vector3(0, -9.81, 0));
}

// Моделює підвіску, зчеплення, тягу й опір одного колеса
void VehicleController::simulateWheel(wheel& wheel)
{
	Transform* transform = getOwner()->getTransform();

	RaycastHit hit;
	Vector3 direction = -getOwner()->getTransform()->getUp();
	Vector3 position = wheel.positionTransform->getPosition();

	if (Physics::raycast(position + getOwner()->getTransform()->getUp() * 0.5f, direction, maxDistance + 0.5f, hit, mRigidBody))
	{
		hit.distance -= 0.5f;

		Vector3 forward = wheel.positionTransform->getForward();

		// Grip

		Vector3 movementDir = mRigidBody->getVelocityAtPoint(position);
		Vector3 wheelForward = forward;

		if (movementDir.length() < 10.00f)
		{
			movementDir = forward;
		}

		movementDir.y = 0.0f;
		wheelForward.y = 0.0f;

		movementDir.normalize();
		wheelForward.normalize();

		float slip = movementDir * wheelForward;

		if (slip < 0.0f) slip = -slip;
		if (slip < 0.5f) slip = 0.5f;

		float grip = 6.0f * slip * slip * gripRatio;

		// Suspension
		float velocity = mRigidBody->getVelocityAtPoint(position) * -direction;
		float springForce = (maxDistance - hit.distance) / maxDistance * this->force - velocity * damping;
		mRigidBody->addForce(-direction * springForce, hit.point - direction * 0.3f);

		// Steering
		Vector3 up = Vector3(0, 1, 0);
		Vector3 lateral = up.cross(forward).normalized();

		Vector3 wheelVel = mRigidBody->getVelocityAtPoint(hit.point);
		float latSpeed = lateral * wheelVel;

		Vector3 frictionForce = -lateral * (latSpeed * grip);

		mRigidBody->addForce(frictionForce, hit.point);

		// Acceleration
		if (wheel.acceleration != 0.0f)
		{
			Vector3 accelerationForce = forward * wheel.acceleration;
			mRigidBody->addForce(accelerationForce, hit.point);
		}

		wheel.spinSpeed = wheelVel * forward * 0.04f;
		wheel.spin += wheel.spinSpeed;

		float maxDiff = 0.01f;
		float distanceDiff = (hit.distance - 0.3f) - wheel.springDistance;
		if (distanceDiff > maxDiff) distanceDiff = maxDiff;
		wheel.springDistance += distanceDiff;

		wheel.wheelTransform->setLocalRotation(Vector3(wheel.spin * (wheel.rightSide ? 1.0f : -1.0f), 
			wheel.rightSide ?  0.0f: 3.1415f, 0.0f));

		wheel.wheelTransform->setLocalPosition(-Vector3(0, 1, 0) * wheel.springDistance);

		// Drag
		float wheelSpeed = wheelVel * forward;

		float dragForce = wheelSpeed;
		if (dragForce > 0.5f) dragForce = 0.5f;
		if (dragForce < -2.0f) dragForce = -2.0f;

		mRigidBody->addForce(-forward * dragForce, hit.point);
	}
	else
	{
		wheel.spin += wheel.spinSpeed;
		wheel.spinSpeed *= 0.995f;

		float maxDiff = 0.01f;
		float distanceDiff = (maxDistance - 0.3f) - wheel.springDistance;
		if (distanceDiff > maxDiff) distanceDiff = maxDiff;
		wheel.springDistance += distanceDiff;

		wheel.wheelTransform->setLocalRotation(Vector3(wheel.spin * (wheel.rightSide ? 1.0f : -1.0f),
			wheel.rightSide ? 0.0f : 3.1415f, 0.0f));

		wheel.wheelTransform->setLocalPosition(-Vector3(0, 1, 0) * wheel.springDistance);
	}
}

// Створює колесо з образу в заданій точці кріплення
void VehicleController::initWheel(wheel& wheel, Vector3 position)
{
	wheel.positionTransform = (new Entity())->getTransform();
	wheel.positionTransform->getOwner()->setParent(mOwner);
	wheel.positionTransform->setLocalPosition(position);
	wheel.wheelTransform = wheelPrefab->instantiate()->getTransform();
	wheel.wheelTransform->getOwner()->setParent(wheel.positionTransform->getOwner());
}

// Перелічує власні поля для файлу сцени та інспектора
void VehicleController::visitProperties(PropertyVisitor& visitor)
{
	visitor.property("id", id);
	visitor.property("force", force, 0.5f);
	visitor.property("damping", damping, 0.1f);
	visitor.property("maxDistance", maxDistance, 0.01f);
	visitor.property("maxSpeed", maxSpeed, 0.5f);
	visitor.property("maxSteering", maxSteering, 0.01f);
	visitor.property("gripRatio", gripRatio, 0.01f);
	visitor.property("maxThrust", maxThrust, 0.1f);
	visitor.property("playerInput", playerInput);
	visitor.property("debugWindow", debugWindow);

	// Без образу колеса машина після завантаження лишилася б без коліс
	visitor.reference("wheelPrefab", wheelPrefab);
}
