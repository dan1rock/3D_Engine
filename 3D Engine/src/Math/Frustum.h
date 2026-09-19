#pragma once
#include "Matrix.h"
#include "Vector3.h"

// Піраміда видимості, задана шістьма площинами. Площини виводяться з матриці виду та проекції,
// тому однаково працює і для перспективи камери, і для ортографії каскаду тіней
class Frustum
{
public:
	Frustum() {

	}

	// Будує площини з готової матриці виду та проекції
	Frustum(const Matrix& viewProjection) {
		build(viewProjection);
	}

	// Виділяє площини з матриці: кожна є сумою або різницею її стовпців
	void build(const Matrix& matrix) {
		// Точка всередині задовольняє -w <= x <= w, -w <= y <= w та 0 <= z <= w
		setPlane(0,
			matrix.mat[0][3] + matrix.mat[0][0], matrix.mat[1][3] + matrix.mat[1][0],
			matrix.mat[2][3] + matrix.mat[2][0], matrix.mat[3][3] + matrix.mat[3][0]);
		setPlane(1,
			matrix.mat[0][3] - matrix.mat[0][0], matrix.mat[1][3] - matrix.mat[1][0],
			matrix.mat[2][3] - matrix.mat[2][0], matrix.mat[3][3] - matrix.mat[3][0]);
		setPlane(2,
			matrix.mat[0][3] + matrix.mat[0][1], matrix.mat[1][3] + matrix.mat[1][1],
			matrix.mat[2][3] + matrix.mat[2][1], matrix.mat[3][3] + matrix.mat[3][1]);
		setPlane(3,
			matrix.mat[0][3] - matrix.mat[0][1], matrix.mat[1][3] - matrix.mat[1][1],
			matrix.mat[2][3] - matrix.mat[2][1], matrix.mat[3][3] - matrix.mat[3][1]);

		// Ближня площина в Direct3D відповідає нулю, а не мінус одиниці
		setPlane(4,
			matrix.mat[0][2], matrix.mat[1][2],
			matrix.mat[2][2], matrix.mat[3][2]);
		setPlane(5,
			matrix.mat[0][3] - matrix.mat[0][2], matrix.mat[1][3] - matrix.mat[1][2],
			matrix.mat[2][3] - matrix.mat[2][2], matrix.mat[3][3] - matrix.mat[3][2]);
	}

	// Перевіряє, чи перетинає сфера піраміду
	// sidesOnly залишає лише бічні площини: для тіней об'єкт поза межами піраміди вздовж
	// напрямку світла все одно може кидати в неї тінь
	bool intersects(const Vector3& center, float radius, bool sidesOnly = false) const {
		int count = sidesOnly ? 4 : 6;

		for (int i = 0; i < count; i++) {
			float distance = mPlanes[i][0] * center.x + mPlanes[i][1] * center.y
				+ mPlanes[i][2] * center.z + mPlanes[i][3];

			// Сфера повністю за площиною, тож і поза пірамідою
			if (distance < -radius) return false;
		}

		return true;
	}

private:
	// Зберігає площину, нормалізуючи її так, щоб відстань вимірювалась у світових одиницях
	void setPlane(int index, float a, float b, float c, float d) {
		float length = sqrtf(a * a + b * b + c * c);

		if (length > 0.000001f) {
			a /= length; b /= length; c /= length; d /= length;
		}

		mPlanes[index][0] = a;
		mPlanes[index][1] = b;
		mPlanes[index][2] = c;
		mPlanes[index][3] = d;
	}

	float mPlanes[6][4] = {};
};
