#pragma once

class Vector2
{
public:
	Vector2(): x(0), y(0)
	{

	}

	Vector2(float coordX, float coordY): x(coordX), y(coordY)
	{
	
	}

	Vector2(const Vector2& vector) : x(vector.x), y(vector.y)
	{

	}

	bool operator==(const Vector2& vector) const
	{
		return (x == vector.x && y == vector.y);
	}

	float x, y;
};
