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

	// Конструктор копіювання
	Vector3(const Vector3& vector) : x(vector.x), y(vector.y), z(vector.z) {

	}

	// Оператор порівняння векторів
	bool operator ==(const Vector3& vector) const {
		return (x == vector.x && y == vector.y && z == vector.z);
	}

	// Оператор множення вектора на скаляр
	Vector3 operator *(float num) {
		return Vector3(x * num, y * num, z * num);
	}

	// Оператор множення з присвоєнням на скаляр
	void operator *=(float num) {
		x *= num;
		y *= num;
		z *= num;
	}

	// Оператор додавання векторів
	Vector3 operator +(Vector3 vector) {
		return Vector3(x + vector.x, y + vector.y, z + vector.z);
	}

	// Оператор віднімання векторів
	Vector3 operator -(Vector3 vector) {
		return Vector3(x - vector.x, y - vector.y, z - vector.z);
	}

	// Оператор зміни знаку вектора
	Vector3 operator -() const {
		return Vector3(-x, -y, -z);
	}

	// Обчислює векторний добуток з іншим вектором
	Vector3 cross(const Vector3& vector) {
		return Vector3(
			y * vector.z - z * vector.y,
			z * vector.x - x * vector.z,
			x * vector.y - y * vector.x);
	}

	// Оператор скалярного добутку з іншим вектором
	float operator *(const Vector3& vector) const {
		return x * vector.x + y * vector.y + z * vector.z;
	}

	// Нормалізує вектор
	void normalize() {
		float length = (float)sqrt(x * x + y * y + z * z);
		if (length != 0) {
			x /= length;
			y /= length;
			z /= length;
		}
	}

	// Повертає нормалізовану копію вектора
	Vector3 normalized() const {
		Vector3 res = Vector3(*this);
		res.normalize();
		return res;
	}

	// Повертає довжину (модуль) вектора
	float length() const {
		return (float)sqrt(x * x + y * y + z * z);
	}

	// Лінійна інтерполяція між двома векторами
	static Vector3 lerp(const Vector3& startPos, const Vector3& endPos, float delta) {
		Vector3 res;
		res.x = startPos.x * (1.0f - delta) + endPos.x * delta;
		res.y = startPos.y * (1.0f - delta) + endPos.y * delta;
		res.z = startPos.z * (1.0f - delta) + endPos.z * delta;
		return res;
	}

	// Повертає вектор, який є результатом обертання навколо вектора "вгору" на заданий кут
	static Vector3 rotateAroundUp(const Vector3& forward, const Vector3& up, float angleRadians) {
		Vector3 f = forward.normalized();
		Vector3 k = up.normalized();
		float c = cos(angleRadians);
		float s = sin(angleRadians);

		return f * c
			+ (k.cross(f)) * s
			+ k * ((k * f) * (1.0f - c));
	}

	~Vector3() 
	{
	
	}

	float x, y, z;
};
