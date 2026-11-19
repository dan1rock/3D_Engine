#include "NeuralNetwork.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Підпис і версія формату файлу мережі; нова версія потрібна при будь-якій зміні формату
static const char NETWORK_MAGIC[4] = { 'E', 'N', 'N', '1' };
static const int NETWORK_VERSION = 1;

const char* NeuralNetwork::FILE_EXTENSION = ".nn";

// Створює порожню мережу без шарів
NeuralNetwork::NeuralNetwork()
{
}

// Створює мережу із заданими розмірами шарів, від входу до виходу
NeuralNetwork::NeuralNetwork(const std::vector<int>& layers)
{
	build(layers);
}

// Перебудовує мережу під нові розміри шарів; параметри обнуляються
void NeuralNetwork::build(const std::vector<int>& layers)
{
	mLayers = layers;

	int count = 0;
	int widest = 0;

	for (size_t i = 0; i < mLayers.size(); i++)
	{
		if (mLayers[i] > widest) widest = mLayers[i];
		if (i > 0) count += mLayers[i - 1] * mLayers[i] + mLayers[i];
	}

	mParameters.assign(count, 0.0f);
	mBufferA.assign(widest, 0.0f);
	mBufferB.assign(widest, 0.0f);
}

// Обчислює вихід мережі для вхідного вектора; розміри мають відповідати першому та останньому шарам
void NeuralNetwork::forward(const float* input, float* output) const
{
	if (mLayers.size() < 2) return;

	std::memcpy(mBufferA.data(), input, sizeof(float) * mLayers[0]);

	const float* p = mParameters.data();
	float* in = mBufferA.data();
	float* out = mBufferB.data();

	for (size_t l = 1; l < mLayers.size(); l++)
	{
		int inSize = mLayers[l - 1];
		int outSize = mLayers[l];

		const float* weights = p;
		const float* biases = p + inSize * outSize;

		for (int o = 0; o < outSize; o++)
		{
			const float* row = weights + o * inSize;
			float sum = biases[o];

			for (int i = 0; i < inSize; i++) sum += row[i] * in[i];

			out[o] = std::tanh(sum);
		}

		p = biases + outSize;
		std::swap(in, out);
	}

	std::memcpy(output, in, sizeof(float) * mLayers.back());
}

// Заповнює ваги випадково за Ксав'є, а зсуви нулями
void NeuralNetwork::randomize(std::mt19937& rng)
{
	float* p = mParameters.data();

	for (size_t l = 1; l < mLayers.size(); l++)
	{
		int inSize = mLayers[l - 1];
		int outSize = mLayers[l];

		// Межа Ксав'є зберігає дисперсію сигналу між шарами, тож тангенс не насичується одразу
		float limit = std::sqrt(6.0f / (float)(inSize + outSize));
		std::uniform_real_distribution<float> uniform(-limit, limit);

		for (int i = 0; i < inSize * outSize; i++) *p++ = uniform(rng);
		for (int i = 0; i < outSize; i++) *p++ = 0.0f;
	}
}

// Повертає розміри шарів
const std::vector<int>& NeuralNetwork::getLayers() const
{
	return mLayers;
}

// Повертає кількість входів
int NeuralNetwork::getInputSize() const
{
	return mLayers.empty() ? 0 : mLayers.front();
}

// Повертає кількість виходів
int NeuralNetwork::getOutputSize() const
{
	return mLayers.empty() ? 0 : mLayers.back();
}

// Повертає загальну кількість параметрів (ваг і зсувів)
int NeuralNetwork::getParameterCount() const
{
	return (int)mParameters.size();
}

// Повертає всі параметри одним вектором: для кожного шару спершу ваги за рядками, потім зсуви
const std::vector<float>& NeuralNetwork::getParameters() const
{
	return mParameters;
}

// Замінює всі параметри; розмір вектора має збігатися з кількістю параметрів
bool NeuralNetwork::setParameters(const std::vector<float>& parameters)
{
	if (parameters.size() != mParameters.size()) return false;

	mParameters = parameters;
	return true;
}

// Записує мережу у файл ресурсу разом із поколінням і пристосованістю, з якими її отримано
bool NeuralNetwork::save(const std::string& path, int generation, float fitness) const
{
	std::error_code error;
	filesystem::path file(path);

	if (file.has_parent_path()) filesystem::create_directories(file.parent_path(), error);

	// Спершу пишемо тимчасовий файл: агент, що саме читає мережу, не побачить її наполовину записаною
	std::string temporary = path + ".tmp";

	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);

		if (!out) return false;

		int layerCount = (int)mLayers.size();
		int parameterCount = (int)mParameters.size();

		out.write(NETWORK_MAGIC, sizeof(NETWORK_MAGIC));
		out.write((const char*)&NETWORK_VERSION, sizeof(int));
		out.write((const char*)&layerCount, sizeof(int));
		out.write((const char*)mLayers.data(), sizeof(int) * layerCount);
		out.write((const char*)&generation, sizeof(int));
		out.write((const char*)&fitness, sizeof(float));
		out.write((const char*)&parameterCount, sizeof(int));
		out.write((const char*)mParameters.data(), sizeof(float) * parameterCount);

		if (!out) return false;
	}

	filesystem::rename(temporary, file, error);

	return !error;
}

// Читає мережу з файлу ресурсу; generation і fitness отримують збережені значення, якщо задані
bool NeuralNetwork::load(const std::string& path, int* generation, float* fitness)
{
	std::ifstream in(path, std::ios::binary);

	if (!in) return false;

	char magic[4] = {};
	int version = 0;
	int layerCount = 0;

	in.read(magic, sizeof(magic));
	in.read((char*)&version, sizeof(int));
	in.read((char*)&layerCount, sizeof(int));

	if (!in || std::memcmp(magic, NETWORK_MAGIC, sizeof(magic)) != 0 || version != NETWORK_VERSION) return false;
	if (layerCount < 2 || layerCount > 64) return false;

	std::vector<int> layers(layerCount);
	in.read((char*)layers.data(), sizeof(int) * layerCount);

	for (int size : layers)
	{
		if (size <= 0 || size > 65536) return false;
	}

	int storedGeneration = 0;
	float storedFitness = 0.0f;
	int parameterCount = 0;

	in.read((char*)&storedGeneration, sizeof(int));
	in.read((char*)&storedFitness, sizeof(float));
	in.read((char*)&parameterCount, sizeof(int));

	NeuralNetwork loaded(layers);

	if (!in || parameterCount != loaded.getParameterCount()) return false;

	in.read((char*)loaded.mParameters.data(), sizeof(float) * parameterCount);

	if (!in) return false;

	*this = loaded;

	if (generation) *generation = storedGeneration;
	if (fitness) *fitness = storedFitness;

	return true;
}
