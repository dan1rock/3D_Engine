#pragma once

class Vector4
{
public:
	Vector4() : x(0), y(0), z(0), w(0)
	{

	}

	Vector4(float coordX, float coordY, float coordZ, float coordW) : x(coordX), y(coordY), z(coordZ), w(coordW) 
	{

	}

	// Конструктор копіювання
	Vector4(const Vector4& vector) : x(vector.x), y(vector.y), z(vector.z), w(vector.w) 
	{

	}

	// Обчислює векторний добуток з іншим вектором
	void cross(Vector4& v1, Vector4& v2, Vector4& v3)
	{
		this->x = v1.y * (v2.z * v3.w - v3.z * v2.w) - v1.z * (v2.y * v3.w - v3.y * v2.w) + v1.w * (v2.y * v3.z - v2.z * v3.y);
		this->y = -(v1.x * (v2.z * v3.w - v3.z * v2.w) - v1.z * (v2.x * v3.w - v3.x * v2.w) + v1.w * (v2.x * v3.z - v3.x * v2.z));
		this->z = v1.x * (v2.y * v3.w - v3.y * v2.w) - v1.y * (v2.x * v3.w - v3.x * v2.w) + v1.w * (v2.x * v3.y - v3.x * v2.y);
		this->w = -(v1.x * (v2.y * v3.z - v3.y * v2.z) - v1.y * (v2.x * v3.z - v3.x * v2.z) + v1.z * (v2.x * v3.y - v3.x * v2.y));
	}

	~Vector4()
	{

	}

	float x, y, z, w;
};
