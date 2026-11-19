#pragma once
#include <vector>
#include <string>
#include <random>

// Повнозв'язна нейронна мережа (багатошаровий перцептрон) з гіперболічним тангенсом на кожному шарі
class NeuralNetwork
{
public:
	// Створює порожню мережу без шарів
	NeuralNetwork();
	// Створює мережу із заданими розмірами шарів, від входу до виходу
	NeuralNetwork(const std::vector<int>& layers);

	// Перебудовує мережу під нові розміри шарів; параметри обнуляються
	void build(const std::vector<int>& layers);

	// Обчислює вихід мережі для вхідного вектора; розміри мають відповідати першому та останньому шарам
	void forward(const float* input, float* output) const;

	// Заповнює ваги випадково за Ксав'є, а зсуви нулями
	void randomize(std::mt19937& rng);

	// Повертає розміри шарів
	const std::vector<int>& getLayers() const;
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

	// Записує мережу у файл ресурсу разом із поколінням і пристосованістю, з якими її отримано
	bool save(const std::string& path, int generation = 0, float fitness = 0.0f) const;
	// Читає мережу з файлу ресурсу; generation і fitness отримують збережені значення, якщо задані
	bool load(const std::string& path, int* generation = nullptr, float* fitness = nullptr);

	// Розширення файлів навчених мереж у ресурсах проєкту
	static const char* FILE_EXTENSION;

private:
	// Розміри шарів від входу до виходу
	std::vector<int> mLayers;
	// Ваги та зсуви всіх шарів підряд
	std::vector<float> mParameters;

	// Вхід поточного шару під час обчислення; змінний, бо обчислення не змінює саму мережу
	mutable std::vector<float> mBufferA;
	// Вихід поточного шару під час обчислення
	mutable std::vector<float> mBufferB;
};
