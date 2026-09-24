#include "EditorCamera.h"
#include "GraphicsEngine.h"
#include "GlobalResources.h"
#include "Input.h"
#include "EngineTime.h"
#include "imgui.h"

EditorCamera::EditorCamera()
{
}

// Обробляє ввід та записує свою матрицю виду у глобальні константи
void EditorCamera::update()
{
	ImGuiIO& io = ImGui::GetIO();

	// Поки курсор над панеллю редактора, рух камери не починається
	bool canStartLooking = !io.WantCaptureMouse;

	if (!mLooking && canStartLooking && Input::getMouseButtonDown(MB_Right))
	{
		mLooking = true;
		Input::hideCursor(true);
	}

	if (mLooking && !Input::getMouseButton(MB_Right))
	{
		mLooking = false;
		Input::hideCursor(false);
	}

	if (mLooking)
	{
		Vector2 delta = Input::getDeltaMousePos();

		mYaw += delta.x * lookSpeed;
		mPitch += delta.y * lookSpeed;

		// Обмеження нахилу не дає камері перевернутися через полюс
		const float limit = 1.5533f;
		if (mPitch > limit) mPitch = limit;
		if (mPitch < -limit) mPitch = -limit;

		// Напрямки руху беруться з поточного повороту камери
		Matrix rotation;
		rotation.setIdentity();
		rotation.setRotation(Vector3(mPitch, mYaw, 0.0f));

		Vector3 forward = rotation.getZDirection();
		Vector3 right = rotation.getXDirection();
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);

		float speed = moveSpeed * Time::getDeltaTime();

		// Shift прискорює переміщення, як і в інших редакторах
		if (Input::getKey(VK_SHIFT)) speed *= 3.0f;

		if (Input::getKey('W')) mPosition = mPosition + forward * speed;
		if (Input::getKey('S')) mPosition = mPosition - forward * speed;
		if (Input::getKey('D')) mPosition = mPosition + right * speed;
		if (Input::getKey('A')) mPosition = mPosition - right * speed;
		if (Input::getKey('E')) mPosition = mPosition + up * speed;
		if (Input::getKey('Q')) mPosition = mPosition - up * speed;
	}

	updateViewMatrix();
}

// Наводить камеру на вказану точку з відстанню, достатньою для огляду об'єкта
void EditorCamera::focusOn(const Vector3& target, float radius)
{
	Matrix rotation;
	rotation.setIdentity();
	rotation.setRotation(Vector3(mPitch, mYaw, 0.0f));

	Vector3 forward = rotation.getZDirection();

	// Відводить камеру назад по її ж напрямку погляду
	float distance = radius * 2.5f + 2.0f;

	Vector3 focus = target;

	mPosition = focus - forward * distance;

	updateViewMatrix();
}

// Повертає позицію камери у світі
const Vector3& EditorCamera::getPosition() const
{
	return mPosition;
}

// Повертає точку перед камерою, у якій редактор створює нові об'єкти
Vector3 EditorCamera::getSpawnPoint(float distance) const
{
	Matrix rotation;
	rotation.setIdentity();
	rotation.setRotation(Vector3(mPitch, mYaw, 0.0f));

	// Без відступу вперед новий об'єкт опинився б усередині самої камери і його не було б видно
	return mPosition + rotation.getZDirection() * distance;
}

// Перераховує матрицю виду з позиції та кутів повороту
void EditorCamera::updateViewMatrix()
{
	constant* constantData = GraphicsEngine::get()->getGlobalResources()->getConstantData();

	Matrix world;
	world.setIdentity();
	world.setRotation(Vector3(mPitch, mYaw, 0.0f));
	world.setTranslation(mPosition);

	// Матриця виду є оберненою до матриці самої камери
	Matrix view = world;
	view.inverse();

	constantData->view = view;

	constantData->cameraPos[0] = mPosition.x;
	constantData->cameraPos[1] = mPosition.y;
	constantData->cameraPos[2] = mPosition.z;

	GraphicsEngine::get()->getGlobalResources()->updateConstantBuffer();
}
