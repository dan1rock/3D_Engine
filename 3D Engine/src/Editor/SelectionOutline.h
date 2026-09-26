#pragma once

class Entity;
class SwapChain;
class VertexShader;
class PixelShader;
class ConstantBuffer;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;

// Обведення вибраного об'єкта, як у Unity. Спершу силуети об'єкта та його нащадків
// малюються в окрему маску, потім повноекранний прохід фарбує пікселі навколо силуету.
// Обведення видно й крізь інші об'єкти, тож вибране не губиться за ними
class SelectionOutline
{
public:
	// Звільняє маску та стани конвеєра
	~SelectionOutline();

	// Завантажує шейдери, створює константний буфер і стани конвеєра
	bool init();

	// Малює обведення об'єкта та його нащадків поверх кадру у вікно swapChain розміром width x height
	void render(SwapChain* swapChain, unsigned int width, unsigned int height, Entity* selected);

private:
	// Перестворює маску, якщо розмір вікна змінився
	bool resizeMask(unsigned int width, unsigned int height);
	// Звільняє текстуру маски разом з її переглядами
	void releaseMask();

	VertexShader* mMaskVertexShader = nullptr;
	PixelShader* mMaskPixelShader = nullptr;
	VertexShader* mFullscreenVertexShader = nullptr;
	PixelShader* mOutlinePixelShader = nullptr;

	// Канал маски для поточного об'єкта
	ConstantBuffer* mMaskBuffer = nullptr;

	ID3D11Texture2D* mMaskTexture = nullptr;
	ID3D11RenderTargetView* mMaskTarget = nullptr;
	ID3D11ShaderResourceView* mMaskView = nullptr;
	unsigned int mMaskWidth = 0;
	unsigned int mMaskHeight = 0;

	// Силуети об'єктів поєднуються за найбільшим значенням, тож вибраний і нащадки не затирають одне одного
	ID3D11BlendState* mMaxBlendState = nullptr;
	// Силует потрібен цілим, навіть якщо його частини затулені, тому глибина вимкнена
	ID3D11DepthStencilState* mNoDepthState = nullptr;
	// Силует не залежить від напрямку обходу трикутників
	ID3D11RasterizerState* mRasterState = nullptr;

	bool mReady = false;
};
