#include "CarComponent.h"
#include "RigidBody.h"
#include "Physics.h"
#include "Prefab.h"
#include "Input.h"
#include "EngineTime.h"
#include "imgui.h"
#include <iostream>

#include <string>
#include "SceneIO.h"

CarComponent::CarComponent()
{
}

CarComponent::~CarComponent()
{
}

void CarComponent::awake()
{
	isStarted = false;
}

// Скидає стан, набутий під час гри, щоб наступний запуск створив колеса заново
void CarComponent::onEditorStop()
{
	isStarted = false;
	mRigidBody = nullptr;

	// Колеса створювала сама гра, тож редактор їх уже знищив
	wheel* wheels[] = { &mWheelFR, &mWheelFL, &mWheelBR, &mWheelBL };

	for (wheel* w : wheels)
	{
		w->wheelTransform = nullptr;
		w->positionTransform = nullptr;
	}
}

void CarComponent::start()
{
	isStarted = true;

	// Сцена, завантажена з файлу, не містить посилання на образ колеса, бо формат їх не зберігає.
	// Без нього чи без фізичного тіла машина просто не вмикається, замість того щоб зламати гру
	if (wheelPrefab == nullptr || getOwner()->getComponent<RigidBody>() == nullptr)
	{
		std::cout << "CarComponent: no wheel prefab or rigid body, car stays disabled" << std::endl;
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

void CarComponent::update()
{
	if (!isStarted)
	{
		start();
	}

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
		newCar->getComponent<CarComponent>()->id = id + 1;
		newCar->getTransform()->setPosition(mOwner->getTransform()->getPosition() + mOwner->getTransform()->getRight() * 3.0f);
	};
	ImGui::End();
}

void CarComponent::fixedUpdate()
{
	if (!mRigidBody) return;

	float carSpeed = mRigidBody->getVelocity() * mOwner->getTransform()->getForward();
	float absoluteSpeed = carSpeed;
	if (absoluteSpeed < 0.0f) absoluteSpeed = -absoluteSpeed;
	mSpeed = absoluteSpeed;

	float accelerationRatio = 1.0f - absoluteSpeed / maxSpeed;

	float targetSteering = 0.0f;

	if (Input::getKey('A'))
	{
		targetSteering -= maxSteering;
	}
	if (Input::getKey('D'))
	{
		targetSteering += maxSteering;
	}

	mSteering += (targetSteering - mSteering) * 0.1f;

	float acceleration = 0.0f;

	if (Input::getKey('W'))
	{
		acceleration += 10.0f;
	}
	if (Input::getKey('S'))
	{
		acceleration += -10.0f;
	}

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

void CarComponent::simulateWheel(wheel& wheel)
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

		wheel.wheelTransform->setLocalRotation(Vector3(wheel.spin * (wheel.rightSide ? -1.0f : 1.0f), 
			wheel.rightSide ? 3.1415f : 0.0f, 0.0f));

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

		wheel.wheelTransform->setLocalRotation(Vector3(wheel.spin * (wheel.rightSide ? -1.0f : 1.0f),
			wheel.rightSide ? 3.1415f : 0.0f, 0.0f));

		wheel.wheelTransform->setLocalPosition(-Vector3(0, 1, 0) * wheel.springDistance);
	}
}

void CarComponent::initWheel(wheel& wheel, Vector3 position)
{
	wheel.positionTransform = (new Entity())->getTransform();
	wheel.positionTransform->getOwner()->setParent(mOwner);
	wheel.positionTransform->setLocalPosition(position);
	wheel.wheelTransform = wheelPrefab->instantiate()->getTransform();
	wheel.wheelTransform->getOwner()->setParent(wheel.positionTransform->getOwner());
}

// Записує власні поля та посилання у файл сцени
void CarComponent::serialize(SceneWriter& writer) const
{
	writer.write("id", id);
	writer.write("force", force);
	writer.write("damping", damping);
	writer.write("maxDistance", maxDistance);
	writer.write("maxSpeed", maxSpeed);
	writer.write("maxSteering", maxSteering);
	writer.write("gripRatio", gripRatio);

	// Без образу колеса машина після завантаження лишилася б без коліс
	writer.writeRef("wheelPrefab", wheelPrefab);
}

// Відновлює власні поля та посилання з файлу сцени
void CarComponent::deserialize(const SceneReader& reader)
{
	id = reader.read("id", id);
	force = reader.read("force", force);
	damping = reader.read("damping", damping);
	maxDistance = reader.read("maxDistance", maxDistance);
	maxSpeed = reader.read("maxSpeed", maxSpeed);
	maxSteering = reader.read("maxSteering", maxSteering);
	gripRatio = reader.read("gripRatio", gripRatio);

	wheelPrefab = reader.readPrefab("wheelPrefab");
}
