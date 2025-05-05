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

	friend Matrix operator*(Matrix lhs, const Matrix& rhs) {
		lhs *= rhs;
		return lhs;
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

	void transpose() {
		Matrix out;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				out.mat[i][j] = mat[j][i];
			}
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
		Vector3 r(mat[0][0], mat[0][1], mat[0][2]);
		Vector3 u(mat[1][0], mat[1][1], mat[1][2]);
		Vector3 f(mat[2][0], mat[2][1], mat[2][2]);

		r.normalize();
		u.normalize();
		f.normalize();

		r *= scale.x;
		u *= scale.y;
		f *= scale.z;

		mat[0][0] = r.x;  mat[0][1] = r.y;  mat[0][2] = r.z;
		mat[1][0] = u.x;  mat[1][1] = u.y;  mat[1][2] = u.z;
		mat[2][0] = f.x;  mat[2][1] = f.y;  mat[2][2] = f.z;
	}

	void setForward(const Vector3& forward)
	{
		Vector3 scale = getScale();

		Vector3 f = forward.normalized();
		Vector3 r = Vector3(0.0f, 1.0f, 0.0f).cross(f).normalized();
		Vector3 u = f.cross(r);

		r *= scale.x;
		u *= scale.y;
		f *= scale.z;

		mat[0][0] = r.x;  mat[0][1] = r.y;  mat[0][2] = r.z;  mat[0][3] = 0;
		mat[1][0] = u.x;  mat[1][1] = u.y;  mat[1][2] = u.z;  mat[1][3] = 0;
		mat[2][0] = f.x;  mat[2][1] = f.y;  mat[2][2] = f.z;  mat[2][3] = 0;
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

	void setRotation(const Vector3& r)
	{
		Vector3 oldTrans = getTranslation();
		Vector3 oldScale = getScale();

		float cx = cosf(r.x), sx = sinf(r.x);
		float cy = cosf(r.y), sy = sinf(r.y);
		float cz = cosf(r.z), sz = sinf(r.z);

		Matrix R;

		R.setIdentity();

		R.mat[0][0] = cy * cz;
		R.mat[0][1] = cy * sz;
		R.mat[0][2] = -sy;

		R.mat[1][0] = sx * sy * cz - cx * sz;
		R.mat[1][1] = sx * sy * sz + cx * cz;
		R.mat[1][2] = sx * cy;

		R.mat[2][0] = cx * sy * cz + sx * sz;
		R.mat[2][1] = cx * sy * sz - sx * cz;
		R.mat[2][2] = cx * cy;

		Matrix res;
		res.setIdentity();
		res.setScale(oldScale);
		res *= R;

		res.setTranslation(oldTrans);

		::memcpy(mat, res.mat, sizeof(float) * 16);
	}

	Vector3 getRotation() {
		Vector3 rotation;
		rotation.x = atan2f(mat[2][1], mat[2][2]);
		rotation.y = atan2f(-mat[2][0], sqrtf(mat[2][1] * mat[2][1] + mat[2][2] * mat[2][2]));
		rotation.z = atan2f(mat[1][0], mat[0][0]);
		return rotation;
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
		return Vector3(
			Vector3(mat[0][0], mat[0][1], mat[0][2]).length(),
			Vector3(mat[1][0], mat[1][1], mat[1][2]).length(),
			Vector3(mat[2][0], mat[2][1], mat[2][2]).length()
		);
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
