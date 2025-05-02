#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include <memory>

class Matrix {
public:
	Matrix() {

	}

	void operator *=(const Matrix matrix) {
		Matrix res;

		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				res.mat[i][j] =
					mat[i][0] * matrix.mat[0][j] + mat[i][1] * matrix.mat[1][j] +
					mat[i][2] * matrix.mat[2][j] + mat[i][3] * matrix.mat[3][j];
			}
		}
		::memcpy(mat, res.mat, sizeof(float) * 16);
	}

	float getDeterminant() {
		Vector4 minor, v1, v2, v3;
		float det;

		v1 = Vector4(this->mat[0][0], this->mat[1][0], this->mat[2][0], this->mat[3][0]);
		v2 = Vector4(this->mat[0][1], this->mat[1][1], this->mat[2][1], this->mat[3][1]);
		v3 = Vector4(this->mat[0][2], this->mat[1][2], this->mat[2][2], this->mat[3][2]);

		minor.cross(v1, v2, v3);
		det = -(this->mat[0][3] * minor.x + this->mat[1][3] * minor.y + this->mat[2][3] * minor.z +
			this->mat[3][3] * minor.w);

		return det;
	}

	void inverse()
	{
		int a, i, j;
		Matrix out;
		Vector4 v, vec[3];
		float det = 0.0f;

		det = this->getDeterminant();
		if (!det) return;
		for (i = 0; i < 4; i++)
		{
			for (j = 0; j < 4; j++)
			{
				if (j != i)
				{
					a = j;
					if (j > i) a = a - 1;
					vec[a].x = (this->mat[j][0]);
					vec[a].y = (this->mat[j][1]);
					vec[a].z = (this->mat[j][2]);
					vec[a].w = (this->mat[j][3]);
				}
			}
			v.cross(vec[0], vec[1], vec[2]);

			out.mat[0][i] = pow(-1.0f, i) * v.x / det;
			out.mat[1][i] = pow(-1.0f, i) * v.y / det;
			out.mat[2][i] = pow(-1.0f, i) * v.z / det;
			out.mat[3][i] = pow(-1.0f, i) * v.w / det;
		}

		::memcpy(mat, out.mat, sizeof(float) * 16);
	}

	void setIdentity() {
		::memset(mat, 0, sizeof(float) * 16);
		mat[0][0] = 1;
		mat[1][1] = 1;
		mat[2][2] = 1;
		mat[3][3] = 1;
	}

	void setTranslation(const Vector3& vector) {
		mat[3][0] = vector.x;
		mat[3][1] = vector.y;
		mat[3][2] = vector.z;
	}

	void setScale(const Vector3& scale) {
		mat[0][0] = scale.x;
		mat[1][1] = scale.y;
		mat[2][2] = scale.z;
	}

	void setRotationX(float x) {
		mat[1][1] = cos(x);
		mat[1][2] = sin(x);
		mat[2][1] = -sin(x);
		mat[2][2] = cos(x);
	}

	void setRotationY(float y) {
		mat[0][0] = cos(y);
		mat[0][2] = -sin(y);
		mat[2][0] = sin(y);
		mat[2][2] = cos(y);
	}

	void setRotationZ(float z) {
		mat[0][0] = cos(z);
		mat[0][1] = sin(z);
		mat[1][0] = -sin(z);
		mat[1][1] = cos(z);
	}

	Vector3 getXDirection() {
		return Vector3(mat[0][0], mat[0][1], mat[0][2]);
	}

	Vector3 getYDirection() {
		return Vector3(mat[1][0], mat[1][1], mat[1][2]);
	}

	Vector3 getZDirection() {
		return Vector3(mat[2][0], mat[2][1], mat[2][2]);
	}

	Vector3 getTranslation() {
		return Vector3(mat[3][0], mat[3][1], mat[3][2]);
	}

	Vector3 getScale() {
		return Vector3(mat[0][0], mat[1][1], mat[2][2]);
	}

	void setOrthoPM(float width, float height, float nearPlane, float farPlane) {
		setIdentity();
		mat[0][0] = 2.0f / width;
		mat[1][1] = 2.0f / height;
		mat[2][2] = 1.0f / (farPlane - nearPlane);
		mat[3][2] = -(nearPlane / (farPlane - nearPlane));
	}

	void setPerspectivePM(float fov, float aspectRatio, float nearPlane, float farPlane) {
		float yScale = 1.0f / tan(fov / 2.0f);
		float xScale = yScale / aspectRatio;
		mat[0][0] = xScale;
		mat[1][1] = yScale;
		mat[2][2] = farPlane / (farPlane - nearPlane);
		mat[2][3] = 1.0f;
		mat[3][2] = (-nearPlane * farPlane) / (farPlane - nearPlane);
	}

	~Matrix() {

	}

	float mat[4][4] = {};
};
