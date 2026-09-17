#pragma once
#include <d3d11.h>

class VertexShader;

// Керує картою тіней напрямленого світла: буфером глибини, у який сцена рендериться з точки зору світла
class ShadowMap
{
public:
	ShadowMap();
	// Ініціалізує карту тіней вказаної роздільності: буфер глибини, семплер, растеризатор та шейдер глибини
	bool init(UINT resolution);
	// Звільняє ресурси карти тіней
	bool release();
	~ShadowMap();

	// Оновлює матриці світла та параметри тіней у глобальних константах
	void update();
	// Встановлює буфер глибини карти тіней як ціль рендеру та готує шейдер глибини
	void begin();
	// Передає заповнену карту тіней піксельним шейдерам
	void end();

	// Встановлює розмір області сцени, яку покриває карта тіней, та глибину цієї області
	void setArea(float area, float depthRange);
	// Встановлює зсуви глибини для боротьби з артефактами самозатінення
	void setBias(float depthBias, float slopeBias);
	// Встановлює силу затінення (0 - тіні не помітні, 1 - повністю затемнені)
	void setStrength(float strength);
	// Вмикає або вимикає рендеринг тіней
	void setEnabled(bool enabled);
	// Перевіряє, чи увімкнено рендеринг тіней
	bool isEnabled();

	// Повертає роздільність карти тіней
	UINT getResolution();

private:
	// Обчислює матрицю виду та проекції світла так, щоб область тіней слідувала за камерою
	void updateLightMatrices();

	UINT mResolution = 2048;

	float mArea = 70.0f;
	float mDepthRange = 200.0f;
	float mDepthBias = 0.0008f;
	float mSlopeBias = 0.0025f;
	float mStrength = 1.0f;

	bool mEnabled = true;

	ID3D11Texture2D* mDepthTexture = nullptr;
	ID3D11DepthStencilView* mDepthStencilView = nullptr;
	ID3D11ShaderResourceView* mShaderResourceView = nullptr;
	ID3D11SamplerState* mComparisonSampler = nullptr;
	ID3D11RasterizerState* mRasterState = nullptr;

	VertexShader* mDepthVertexShader = nullptr;
};
