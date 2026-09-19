#pragma once
#include <d3d11.h>

class SwapChain;
class VertexShader;
class PixelShader;
class ConstantBuffer;

// Проміжна ціль рендеру разом з її переглядами для запису та читання
struct RenderTexture
{
	ID3D11Texture2D* texture = nullptr;
	ID3D11RenderTargetView* renderTargetView = nullptr;
	ID3D11ShaderResourceView* shaderResourceView = nullptr;

	UINT width = 0;
	UINT height = 0;
};

// Виконує постобробку кадру: сцена малюється в окрему текстуру, після чого до неї застосовуються ефекти
class PostProcessing
{
public:
	PostProcessing();
	// Завантажує шейдери, створює константний буфер та стан семплера для повноекранних проходів
	bool init();
	// Звільняє ресурси постобробки
	bool release();
	~PostProcessing();

	// Перестворює проміжні цілі рендеру під новий розмір вікна
	void resize(UINT width, UINT height);

	// Спрямовує рендеринг сцени у проміжну текстуру замість вікна
	void begin(SwapChain* swapChain);
	// Застосовує ефекти до відрендереної сцени та виводить результат у вікно
	void apply(SwapChain* swapChain);

	// Вмикає або вимикає постобробку повністю
	void setEnabled(bool enabled);
	// Перевіряє, чи увімкнено постобробку
	bool isEnabled();

	// Вмикає або вимикає ефект світіння
	void setBloomEnabled(bool enabled);
	// Перевіряє, чи увімкнено ефект світіння
	bool isBloomEnabled();
	// Встановлює яскравість, з якої починається світіння, та м'якість переходу до нього
	void setBloomThreshold(float threshold, float knee);
	// Встановлює силу світіння
	void setBloomIntensity(float intensity);

	// Вмикає або вимикає ефект віньєтки
	void setVignetteEnabled(bool enabled);
	// Перевіряє, чи увімкнено ефект віньєтки
	bool isVignetteEnabled();
	// Встановлює силу затемнення країв кадру та плавність його переходу
	void setVignette(float intensity, float smoothness);
	// Встановлює колір, у який затемнюються краї кадру
	void setVignetteColor(float r, float g, float b);

private:
	// Створює проміжну ціль рендеру вказаного розміру
	bool createRenderTexture(RenderTexture& target, UINT width, UINT height);
	// Звільняє проміжну ціль рендеру
	void releaseRenderTexture(RenderTexture& target);

	// Готує конвеєр до повноекранного проходу та малює трикутник на весь екран
	void drawFullscreen(const RenderTexture& source, PixelShader* pixelShader);
	// Оновлює константний буфер параметрами ефектів
	void updateConstantBuffer(const RenderTexture& source);

	// Готує текстуру світіння: виділяє яскраві місця та розмиває їх
	void renderBloom();

	bool mEnabled = true;

	// Освітлення рушія майже не виходить за межі одиниці, тому поріг світіння лежить нижче за неї
	bool mBloomEnabled = true;
	float mBloomThreshold = 0.35f;
	float mBloomKnee = 0.2f;
	float mBloomIntensity = 0.55f;

	// Сила у межах від нуля до одиниці, плавність задає частку кадру, яку займає перехід
	bool mVignetteEnabled = true;
	float mVignetteIntensity = 0.5f;
	float mVignetteSmoothness = 0.6f;
	float mVignetteColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Кількість проходів розмиття: кожен наступний розширює світіння вдвічі
	static const int mBlurPasses = 3;

	RenderTexture mSceneTexture = {};
	RenderTexture mBloomTextures[2] = {};

	VertexShader* mFullscreenVertexShader = nullptr;
	PixelShader* mExtractPixelShader = nullptr;
	PixelShader* mBlurPixelShader = nullptr;
	PixelShader* mCompositePixelShader = nullptr;

	ConstantBuffer* mConstantBuffer = nullptr;
	ID3D11SamplerState* mSampler = nullptr;
	ID3D11RasterizerState* mRasterState = nullptr;

	// Поточний напрямок та крок розмиття, які потрапляють у константний буфер
	float mBlurDirection[4] = {};
};
