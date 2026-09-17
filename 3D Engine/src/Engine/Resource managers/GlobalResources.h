#pragma once
#include "Matrix.h"

class Material;
class ConstantBuffer;

// Глобальна структура для зберігання константних даних
// Кожен тривимірний вектор доповнено до чотирьох компонент, щоб розкладка відповідала регістрам шейдера
__declspec(align(16))
struct constant {
	Matrix world;
	Matrix model;
	Matrix invTransModel;
	Matrix view;
	Matrix projection;
	Matrix lightViewProjection;
	float cameraPos[4];
	float lightPos[4];
	float lightColor[4];
	float lightDir[4];
	// Параметри тіней: розмір текселя, зсув глибини, зсув по нахилу та сила затінення
	float shadowParams[4];
	unsigned int time;
};

class GlobalResources
{
public:
	GlobalResources();
	// Ініціалізує глобальні ресурси: створює константний буфер і матеріал за замовчуванням
	void init();
	// Звільняє глобальний константний буфер
	~GlobalResources();

	// Оновлює дані в константному буфері
	void updateConstantBuffer();

	// Повертає вказівник на структуру з глобальними константними даними
	constant* getConstantData();
	// Повертає вказівник на глобальний константний буфер
	ConstantBuffer* getConstantBuffer();
	// Повертає вказівник на матеріал за замовчуванням
	Material* getDefaultMaterial();

private:
	constant mConstantData = {};
	ConstantBuffer* mConstantBuffer = nullptr;
	Material* mDefaultMaterial = nullptr;
};

