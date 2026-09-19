#pragma once
#include "Matrix.h"

class Material;
class ConstantBuffer;

// Кількість каскадів карти тіней; те саме значення визначено в шейдерах
#define SHADOW_CASCADE_COUNT 4

// Глобальна структура для зберігання константних даних
// Кожен тривимірний вектор доповнено до чотирьох компонент, щоб розкладка відповідала регістрам шейдера
__declspec(align(16))
struct constant {
	Matrix world;
	Matrix model;
	Matrix invTransModel;
	Matrix view;
	Matrix projection;
	// Матриця світла для кожного каскаду окремо
	Matrix lightViewProjection[SHADOW_CASCADE_COUNT];
	float cameraPos[4];
	float lightPos[4];
	float lightColor[4];
	float lightDir[4];
	// Параметри тіней: розмір текселя, зсув глибини, зсув по нахилу та сила затінення
	float shadowParams[4];
	// Межі каскадів уздовж осі камери
	float cascadeSplits[4];
	// Зсув глибини, підібраний під розмір текселя кожного каскаду
	float cascadeBias[4];
	// Номер каскаду, який зараз рендериться, та ширина зони змішування каскадів
	float cascadeParams[4];
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

