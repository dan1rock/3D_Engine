#pragma once
#include <vector>
#include <string>
#include <random>
#include <iosfwd>

// Повнозв'язна нейронна мережа (багатошаровий перцептрон): гіперболічний тангенс у прихованих шарах, вихідний шар - тангенс або лінійний
class NeuralNetwork
{
public:
	// Створює порожню мережу без шарів
	NeuralNetwork();
	// Створює мережу із заданими розмірами шарів, від входу до виходу
	NeuralNetwork(const std::vector<int>& layers, bool linearOutput = false);

	// Перебудовує мережу під нові розміри шарів; параметри обнуляються
	void build(const std::vector<int>& layers, bool linearOutput = false);

	// Обчислює вихід мережі для вхідного вектора; розміри мають відповідати першому та останньому шарам
	void forward(const float* input, float* output) const;
	// Обчислює виходи для count входів підряд; швидше за окремі виклики, бо ваги обходяться по рядках пам'яті
	void forwardBatch(const float* inputs, int count, float* outputs) const;

	// Додає до gradients градієнт половини квадрата похибки для одного прикладу й повертає середній квадрат похибки виходів
	float accumulateGradient(const float* input, const float* target, float* gradients) const;
	// Для мережі з одним лінійним виходом, що дає логарифм шансів, додає градієнт логістичної похибки й повертає саму похибку
	float accumulateLogisticGradient(const float* input, float target, float* gradients) const;

	// Заповнює ваги випадково за Ксав'є, а зсуви нулями
	void randomize(std::mt19937& rng);

	// Повертає розміри шарів
	const std::vector<int>& getLayers() const;
	// Перевіряє, чи вихідний шар лінійний
	bool isLinearOutput() const;
	// Повертає кількість входів
	int getInputSize() const;
	// Повертає кількість виходів
	int getOutputSize() const;
	// Повертає загальну кількість параметрів (ваг і зсувів)
	int getParameterCount() const;

	// Повертає всі параметри одним вектором: для кожного шару спершу ваги за рядками, потім зсуви
	const std::vector<float>& getParameters() const;
	// Замінює всі параметри; розмір вектора має збігатися з кількістю параметрів
	bool setParameters(const std::vector<float>& parameters);
	// Повертає параметри для зміни на місці, наприклад оптимізатором під час навчання
	float* getParameterData();

	// Записує шари, вид виходу й параметри в потік, щоб мережу можна було вкласти в інший файл ресурсу
	bool write(std::ostream& out) const;
	// Читає мережу, записану write
	bool read(std::istream& in);

	// Записує мережу у файл ресурсу разом із поколінням і пристосованістю, з якими її отримано
	bool save(const std::string& path, int generation = 0, float fitness = 0.0f) const;
	// Читає мережу з файлу ресурсу; generation і fitness отримують збережені значення, якщо задані
	bool load(const std::string& path, int* generation = nullptr, float* fitness = nullptr);

	// Розширення файлів навчених мереж у ресурсах проєкту
	static const char* FILE_EXTENSION;

private:
	// Перебудовує транспоновані ваги для пакетного обчислення, якщо параметри змінилися
	void updateTransposed() const;
	// Обчислює прямий прохід і зберігає виходи всіх шарів, бо від них залежать градієнти
	void forwardTraining(const float* input) const;
	// Поширює похибку виходу з mDelta назад і додає градієнти ваг і зсувів до gradients
	void backpropagate(float* gradients) const;

	// Розміри шарів від входу до виходу
	std::vector<int> mLayers;
	// Ваги та зсуви всіх шарів підряд
	std::vector<float> mParameters;
	// Чи вихідний шар лінійний, як потрібно для передбачення неперервних величин
	bool mLinearOutput = false;

	// Вхід поточного шару під час обчислення; змінний, бо обчислення не змінює саму мережу
	mutable std::vector<float> mBufferA;
	// Вихід поточного шару під час обчислення
	mutable std::vector<float> mBufferB;
	// Виходи всіх шарів одного прикладу для зворотного поширення похибки
	mutable std::vector<std::vector<float>> mActivations;
	// Зсув параметрів кожного шару для зворотного поширення похибки
	mutable std::vector<size_t> mOffsets;
	// Похибки нейронів поточного й попереднього шарів під час зворотного поширення
	mutable std::vector<float> mDelta;
	// Похибки нейронів попереднього шару під час зворотного поширення
	mutable std::vector<float> mPreviousDelta;
	// Ваги кожного шару, транспоновані для пакетного обчислення
	mutable std::vector<float> mTransposed;
	// Чи транспоновані ваги відповідають параметрам
	mutable bool mTransposedValid = false;
	// Проміжні результати пакетного обчислення
	mutable std::vector<float> mBatchA;
	// Проміжні результати пакетного обчислення наступного шару
	mutable std::vector<float> mBatchB;
};
