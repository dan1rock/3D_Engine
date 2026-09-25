#pragma once
#include "Vector3.h"

class Entity;

// Промінь у світових координатах
struct Ray
{
	Vector3 origin;
	Vector3 direction;
};

// Режим маніпулятора вибраного об'єкта
enum class GizmoMode
{
	Translate,
	Rotate,
	Scale
};

// Вибір об'єктів мишею та маніпулятор переміщення, обертання і масштабу.
// Малюється списком команд ImGui поверх сцени, тому не потребує власного шейдера
class Gizmo
{
public:
	// Будує промінь з камери крізь точку екрана
	static Ray screenPointToRay(float screenX, float screenY);
	// Переводить точку світу в координати екрана; повертає false, якщо вона позаду камери
	static bool projectToScreen(const Vector3& world, float& screenX, float& screenY);

	// Шукає найближчий видимий об'єкт під вказаною точкою екрана
	static Entity* pick(float screenX, float screenY, float* hitDistance = nullptr);

	// Малює маніпулятор вибраного об'єкта та обробляє перетягування. Повертає true, поки миша
	// зайнята маніпулятором: тоді клік не повинен міняти вибір
	bool update(Entity* entity);

	// Перевіряє, чи триває перетягування просто зараз
	bool isDragging() const;

	// Поточний режим маніпулятора
	GizmoMode mode = GizmoMode::Translate;
	// Осі маніпулятора: власні осі об'єкта замість світових
	bool local = false;

private:
	// Обробляє перетягування вздовж осі та повертає true, поки воно триває
	bool dragTranslate(Entity* entity);
	bool dragScale(Entity* entity, float size);
	bool dragRotate(Entity* entity);

	// Вісь, за яку тягнуть (0 = X, 1 = Y, 2 = Z), або -1
	int mAxis = -1;
	bool mDragging = false;

	// Стан об'єкта та миші на початку перетягування
	Vector3 mStartPosition;
	Vector3 mStartRotation;
	Vector3 mStartScale;

	// Осі запам'ятовуються на початку перетягування: у локальному режимі вони належать
	// самому об'єкту і міняються разом з ним, що зациклило б обертання
	Vector3 mDragAxis;
	Vector3 mDragFirst;
	Vector3 mDragSecond;
	float mStartOffset = 0.0f;
	float mStartAngle = 0.0f;
};
