#pragma once
#include "Vector3.h"
#include "Matrix.h"

// Камера редактора: літає сценою незалежно від камер, що належать об'єктам
class EditorCamera
{
public:
	EditorCamera();

	// Обробляє ввід та записує свою матрицю виду у глобальні константи
	void update();

	// Наводить камеру на вказану точку з відстанню, достатньою для огляду об'єкта
	void focusOn(const Vector3& target, float radius);

	// Повертає позицію камери у світі
	const Vector3& getPosition() const;

	// Положення й поворот камери, щоб після режиму префаба повернутися туди, де була
	struct View
	{
		Vector3 position;
		float pitch = 0.0f;
		float yaw = 0.0f;
	};

	// Повертає поточне положення камери
	View getView() const;
	// Ставить камеру у збережене положення
	void setView(const View& view);
	// Ставить камеру туди, звідки дивиться вказана матриця виду, щоб зображення не стрибнуло
	void setFromViewMatrix(const Matrix& viewMatrix);

	// Повертає точку перед камерою, у якій редактор створює нові об'єкти
	Vector3 getSpawnPoint(float distance = 8.0f) const;

	// Швидкість переміщення та чутливість огляду, які редактор показує в налаштуваннях
	float moveSpeed = 12.0f;
	float lookSpeed = 0.0025f;

private:
	// Перераховує матрицю виду з позиції та кутів повороту
	void updateViewMatrix();

	Vector3 mPosition = { 0.0f, 6.0f, -12.0f };
	// Кути нахилу та повороту навколо вертикалі
	float mPitch = 0.35f;
	float mYaw = 0.0f;

	// Чи обертається камера просто зараз: поки триває обертання, курсор ховається
	bool mLooking = false;
};
