#pragma once

class SwapChain;
class VertexShader;
class PixelShader;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;

// Сітка на горизонтальній площині XZ, як у сцені Unity. Малюється справжньою геометрією з
// перевіркою глибини сцени, тож об'єкти над площиною її затуляють, а під нею - видно крізь неї
class EditorGrid
{
public:
	// Звільняє створені стани конвеєра
	~EditorGrid();

	// Завантажує шейдери та створює стани змішування, глибини й растеризатора
	bool init();

	// Малює сітку поверх кадру у вікно swapChain, використовуючи глибину відрендереної сцени
	void render(SwapChain* swapChain);

private:
	VertexShader* mVertexShader = nullptr;
	PixelShader* mPixelShader = nullptr;

	// Напівпрозорі лінії змішуються з кадром і не записують глибину, лише перевіряють її
	ID3D11BlendState* mBlendState = nullptr;
	ID3D11DepthStencilState* mDepthState = nullptr;
	// Площину видно і згори, і знизу, тож обидві сторони трикутників малюються
	ID3D11RasterizerState* mRasterState = nullptr;

	bool mReady = false;
};
