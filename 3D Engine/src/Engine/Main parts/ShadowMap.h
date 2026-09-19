#pragma once
#include <d3d11.h>
#include "GlobalResources.h"

class VertexShader;

// Керує каскадною картою тіней: сцена рендериться з точки зору світла окремо для кожного каскаду,
// від найближчого та найдетальнішого до найдальшого
class ShadowMap
{
public:
	ShadowMap();
	// Ініціалізує карту тіней вказаної роздільності: масив буферів глибини, семплер, растеризатор та шейдер глибини
	bool init(UINT resolution);
	// Звільняє ресурси карти тіней
	bool release();
	~ShadowMap();

	// Оновлює матриці світла та параметри тіней у глобальних константах
	void update();
	// Готує спільний стан конвеєра для проходу карти тіней
	void begin();
	// Очищає буфер глибини вказаного каскаду та робить його ціллю рендеру
	void beginCascade(int cascade);
	// Передає заповнену карту тіней піксельним шейдерам
	void end();

	// Встановлює відстань, до якої від камери будуються тіні
	void setShadowDistance(float distance);
	// Встановлює запас глибини, у межах якого об'єкти позаду каскаду ще кидають тінь
	void setCasterDistance(float distance);
	// Встановлює розподіл меж каскадів: 0 - рівномірний, 1 - логарифмічний
	void setSplitLambda(float lambda);
	// Встановлює зсув глибини у світових одиницях та його множник за розміром текселя
	void setBias(float depthBias, float texelBias);
	// Встановлює ширину зони, у якій сусідні каскади плавно змішуються
	void setBlendBand(float band);
	// Встановлює силу затінення (0 - тіні не помітні, 1 - повністю затемнені)
	void setStrength(float strength);
	// Вмикає або вимикає рендеринг тіней
	void setEnabled(bool enabled);
	// Перевіряє, чи увімкнено рендеринг тіней
	bool isEnabled();

	// Повертає роздільність одного каскаду
	UINT getResolution();
	// Повертає кількість каскадів
	int getCascadeCount();

private:
	// Обчислює межі каскадів уздовж осі камери
	void updateCascadeSplits(float nearPlane);
	// Обчислює матрицю виду та проекції світла для одного каскаду
	void updateCascadeMatrix(int cascade, const Vector3& direction, float nearDistance, float farDistance);

	UINT mResolution = 2048;

	float mShadowDistance = 150.0f;
	float mCasterDistance = 50.0f;
	float mSplitLambda = 0.85f;
	float mDepthBias = 0.02f;
	float mTexelBias = 1.5f;
	float mBlendBand = 2.0f;
	float mStrength = 1.0f;

	bool mEnabled = true;

	ID3D11Texture2D* mDepthTexture = nullptr;
	ID3D11DepthStencilView* mDepthStencilViews[SHADOW_CASCADE_COUNT] = {};
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;
	ID3D11SamplerState* mComparisonSampler = nullptr;
	ID3D11RasterizerState* mRasterState = nullptr;

	VertexShader* mDepthVertexShader = nullptr;

	// Ближня та дальня межа кожного каскаду вздовж осі камери
	float mSplits[SHADOW_CASCADE_COUNT + 1] = {};
};
