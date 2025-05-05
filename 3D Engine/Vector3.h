#pragma once
#include "cmath"

class Vector3 
{
public:
	Vector3(): x(0), y(0), z(0)
	{

	}

	Vector3(float coordX, float coordY, float coordZ) : x(coordX), y(coordY), z(coordZ) {

	}

	Vector3(const Vector3& vector) : x(vector.x), y(vector.y), z(vector.z) {

	}

	bool operator ==(const Vector3& vector) const {
		return (x == vector.x && y == vector.y && z == vector.z);
	}

	Vector3 operator *(float num) {
		return Vector3(x * num, y * num, z * num);
	}

	void operator *=(float num) {
		x *= num;
		y *= num;
		z *= num;
	}

	Vector3 operator +(Vector3 vector) {
		return Vector3(x + vector.x, y + vector.y, z + vector.z);
	}

	Vector3 cross(const Vector3& vector) {
		return Vector3(
			y * vector.z - z * vector.y,
			z * vector.x - x * vector.z,
			x * vector.y - y * vector.x);
	}

	void normalize() {
		float length = (float)sqrt(x * x + y * y + z * z);
		if (length != 0) {
			x /= length;
			y /= length;
			z /= length;
		}
	}

	Vector3 normalized() const {
		Vector3 res = Vector3(*this);
		res.normalize();
		return res;
	}

	float length() const {
		return (float)sqrt(x * x + y * y + z * z);
	}

	static Vector3 lerp(const Vector3& startPos, const Vector3& endPos, float delta) {
		Vector3 res;
		res.x = startPos.x * (1.0f - delta) + endPos.x * delta;
		res.y = startPos.y * (1.0f - delta) + endPos.y * delta;
		res.z = startPos.z * (1.0f - delta) + endPos.z * delta;
		return res;
	}

	~Vector3() 
	{
	
	}

	float x, y, z;
};
