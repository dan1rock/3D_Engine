#include "NeuralNetwork.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <fstream>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Підпис формату файлу мережі
static const char NETWORK_MAGIC[4] = { 'E', 'N', 'N', '1' };
// Версія формату: 1 - без виду виходу, 2 - з ним; нова версія потрібна при будь-якій зміні формату
static const int NETWORK_VERSION = 2;

const char* NeuralNetwork::FILE_EXTENSION = ".nn";

// Створює порожню мережу без шарів
NeuralNetwork::NeuralNetwork()
{
}

// Створює мережу із заданими розмірами шарів, від входу до виходу
NeuralNetwork::NeuralNetwork(const std::vector<int>& layers, bool linearOutput)
{
	build(layers, linearOutput);
}

// Перебудовує мережу під нові розміри шарів; параметри обнуляються
void NeuralNetwork::build(const std::vector<int>& layers, bool linearOutput)
{
	mLayers = layers;
	mLinearOutput = linearOutput;

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
	mTransposedValid = false;
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
		bool linear = mLinearOutput && l + 1 == mLayers.size();

		const float* weights = p;
		const float* biases = p + inSize * outSize;

		for (int o = 0; o < outSize; o++)
		{
			const float* row = weights + o * inSize;
			float sum = biases[o];

			for (int i = 0; i < inSize; i++) sum += row[i] * in[i];

			out[o] = linear ? sum : std::tanh(sum);
		}

		p = biases + outSize;
		std::swap(in, out);
	}

	std::memcpy(output, in, sizeof(float) * mLayers.back());
}

// Перебудовує транспоновані ваги для пакетного обчислення, якщо параметри змінилися
void NeuralNetwork::updateTransposed() const
{
	if (mTransposedValid) return;

	mTransposed.resize(mParameters.size());

	const float* p = mParameters.data();
	float* t = mTransposed.data();

	for (size_t l = 1; l < mLayers.size(); l++)
	{
		int inSize = mLayers[l - 1];
		int outSize = mLayers[l];

		// Ваги шару зберігаються за входами: для кожного входу - внесок у всі виходи підряд
		for (int o = 0; o < outSize; o++)
		{
			for (int i = 0; i < inSize; i++) t[i * outSize + o] = p[o * inSize + i];
		}

		std::memcpy(t + inSize * outSize, p + inSize * outSize, sizeof(float) * outSize);

		p += inSize * outSize + outSize;
		t += inSize * outSize + outSize;
	}

	mTransposedValid = true;
}

// Обчислює виходи для count входів підряд; швидше за окремі виклики, бо ваги обходяться по рядках пам'яті
void NeuralNetwork::forwardBatch(const float* inputs, int count, float* outputs) const
{
	if (mLayers.size() < 2 || count <= 0) return;

	updateTransposed();

	int widest = *std::max_element(mLayers.begin(), mLayers.end());
	mBatchA.resize((size_t)widest * count);
	mBatchB.resize((size_t)widest * count);

	std::memcpy(mBatchA.data(), inputs, sizeof(float) * mLayers[0] * count);

	const float* t = mTransposed.data();
	float* in = mBatchA.data();
	float* out = mBatchB.data();

	for (size_t l = 1; l < mLayers.size(); l++)
	{
		int inSize = mLayers[l - 1];
		int outSize = mLayers[l];
		bool linear = mLinearOutput && l + 1 == mLayers.size();

		const float* biases = t + inSize * outSize;

		for (int s = 0; s < count; s++)
		{
			const float* x = in + (size_t)s * inSize;
			float* y = out + (size_t)s * outSize;

			std::memcpy(y, biases, sizeof(float) * outSize);

			// Кожен вхід додає свій стовпець ваг до всіх виходів; такий цикл компілятор векторизує
			for (int i = 0; i < inSize; i++)
			{
				float value = x[i];
				const float* column = t + (size_t)i * outSize;

				for (int o = 0; o < outSize; o++) y[o] += column[o] * value;
			}

			if (!linear)
			{
				for (int o = 0; o < outSize; o++) y[o] = std::tanh(y[o]);
			}
		}

		t = biases + outSize;
		std::swap(in, out);
	}

	std::memcpy(outputs, in, sizeof(float) * mLayers.back() * count);
}

// Додає до gradients градієнт половини квадрата похибки для одного прикладу й повертає середній квадрат похибки виходів
float NeuralNetwork::accumulateGradient(const float* input, const float* target, float* gradients) const
{
	size_t layerCount = mLayers.size();

	if (layerCount < 2) return 0.0f;

	mActivations.resize(layerCount);

	for (size_t l = 0; l < layerCount; l++) mActivations[l].resize(mLayers[l]);

	std::memcpy(mActivations[0].data(), input, sizeof(float) * mLayers[0]);

	// Прямий прохід зберігає виходи всіх шарів, бо від них залежать градієнти
	std::vector<size_t> offsets(layerCount, 0);
	size_t offset = 0;

	for (size_t l = 1; l < layerCount; l++)
	{
		offsets[l] = offset;

		int inSize = mLayers[l - 1];
		int outSize = mLayers[l];
		bool linear = mLinearOutput && l + 1 == layerCount;

		const float* weights = mParameters.data() + offset;
		const float* biases = weights + inSize * outSize;
		const float* in = mActivations[l - 1].data();
		float* out = mActivations[l].data();

		for (int o = 0; o < outSize; o++)
		{
			const float* row = weights + o * inSize;
			float sum = biases[o];

			for (int i = 0; i < inSize; i++) sum += row[i] * in[i];

			out[o] = linear ? sum : std::tanh(sum);
		}

		offset += inSize * outSize + outSize;
	}

	// Похибка виходу; похідна тангенса виражається через сам вихід: 1 - y²
	int outputs = mLayers.back();
	const float* result = mActivations.back().data();
	float squared = 0.0f;

	mDelta.resize(outputs);

	for (int o = 0; o < outputs; o++)
	{
		float error = result[o] - target[o];
		squared += error * error;
		mDelta[o] = mLinearOutput ? error : error * (1.0f - result[o] * result[o]);
	}

	// Зворотний прохід: градієнти ваг шару й похибки попереднього шару
	for (size_t l = layerCount - 1; l >= 1; l--)
	{
		int inSize = mLayers[l - 1];
		int outSize = mLayers[l];

		const float* weights = mParameters.data() + offsets[l];
		float* weightGradients = gradients + offsets[l];
		float* biasGradients = weightGradients + inSize * outSize;
		const float* in = mActivations[l - 1].data();

		for (int o = 0; o < outSize; o++)
		{
			float delta = mDelta[o];
			float* row = weightGradients + o * inSize;

			for (int i = 0; i < inSize; i++) row[i] += delta * in[i];

			biasGradients[o] += delta;
		}

		if (l == 1) break;

		mPreviousDelta.assign(inSize, 0.0f);

		for (int o = 0; o < outSize; o++)
		{
			float delta = mDelta[o];
			const float* row = weights + o * inSize;

			for (int i = 0; i < inSize; i++) mPreviousDelta[i] += row[i] * delta;
		}

		// Попередній шар прихований, тож його похибку множимо на похідну тангенса
		for (int i = 0; i < inSize; i++) mPreviousDelta[i] *= 1.0f - in[i] * in[i];

		mDelta.swap(mPreviousDelta);
	}

	return squared / (float)outputs;
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

	mTransposedValid = false;
}

// Повертає розміри шарів
const std::vector<int>& NeuralNetwork::getLayers() const
{
	return mLayers;
}

// Перевіряє, чи вихідний шар лінійний
bool NeuralNetwork::isLinearOutput() const
{
	return mLinearOutput;
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
	mTransposedValid = false;
	return true;
}

// Повертає параметри для зміни на місці, наприклад оптимізатором під час навчання
float* NeuralNetwork::getParameterData()
{
	// Той, хто змінює параметри напряму, робить транспоновану копію застарілою
	mTransposedValid = false;
	return mParameters.data();
}

// Записує шари, вид виходу й параметри в потік, щоб мережу можна було вкласти в інший файл ресурсу
bool NeuralNetwork::write(std::ostream& out) const
{
	int layerCount = (int)mLayers.size();
	int linear = mLinearOutput ? 1 : 0;
	int parameterCount = (int)mParameters.size();

	out.write((const char*)&layerCount, sizeof(int));
	out.write((const char*)mLayers.data(), sizeof(int) * layerCount);
	out.write((const char*)&linear, sizeof(int));
	out.write((const char*)&parameterCount, sizeof(int));
	out.write((const char*)mParameters.data(), sizeof(float) * parameterCount);

	return (bool)out;
}

// Читає шари й параметри; linear задає вид виходу, якщо формат його не зберігає
static bool readNetworkBody(std::istream& in, std::vector<int>& layers, std::vector<float>& parameters, bool withLinearFlag, bool& linear)
{
	int layerCount = 0;
	in.read((char*)&layerCount, sizeof(int));

	if (!in || layerCount < 2 || layerCount > 64) return false;

	layers.resize(layerCount);
	in.read((char*)layers.data(), sizeof(int) * layerCount);

	for (int size : layers)
	{
		if (size <= 0 || size > 65536) return false;
	}

	if (withLinearFlag)
	{
		int flag = 0;
		in.read((char*)&flag, sizeof(int));
		linear = flag != 0;
	}

	int parameterCount = 0;
	in.read((char*)&parameterCount, sizeof(int));

	if (!in || parameterCount < 0 || parameterCount > 64 * 1024 * 1024) return false;

	parameters.resize(parameterCount);
	in.read((char*)parameters.data(), sizeof(float) * parameterCount);

	return (bool)in;
}

// Читає мережу, записану write
bool NeuralNetwork::read(std::istream& in)
{
	std::vector<int> layers;
	std::vector<float> parameters;
	bool linear = false;

	if (!readNetworkBody(in, layers, parameters, true, linear)) return false;

	NeuralNetwork loaded(layers, linear);

	if (!loaded.setParameters(parameters)) return false;

	*this = loaded;
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

		out.write(NETWORK_MAGIC, sizeof(NETWORK_MAGIC));
		out.write((const char*)&NETWORK_VERSION, sizeof(int));
		out.write((const char*)&generation, sizeof(int));
		out.write((const char*)&fitness, sizeof(float));

		if (!write(out)) return false;
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

	in.read(magic, sizeof(magic));
	in.read((char*)&version, sizeof(int));

	if (!in || std::memcmp(magic, NETWORK_MAGIC, sizeof(magic)) != 0) return false;

	std::vector<int> layers;
	std::vector<float> parameters;
	bool linear = false;
	int storedGeneration = 0;
	float storedFitness = 0.0f;

	if (version == 1)
	{
		// Перша версія: шари, потім покоління й пристосованість, вихід завжди тангенс
		int layerCount = 0;
		in.read((char*)&layerCount, sizeof(int));

		if (!in || layerCount < 2 || layerCount > 64) return false;

		layers.resize(layerCount);
		in.read((char*)layers.data(), sizeof(int) * layerCount);
		in.read((char*)&storedGeneration, sizeof(int));
		in.read((char*)&storedFitness, sizeof(float));

		int parameterCount = 0;
		in.read((char*)&parameterCount, sizeof(int));

		if (!in || parameterCount < 0 || parameterCount > 64 * 1024 * 1024) return false;

		parameters.resize(parameterCount);
		in.read((char*)parameters.data(), sizeof(float) * parameterCount);

		if (!in) return false;
	}
	else if (version == 2)
	{
		in.read((char*)&storedGeneration, sizeof(int));
		in.read((char*)&storedFitness, sizeof(float));

		if (!readNetworkBody(in, layers, parameters, true, linear)) return false;
	}
	else
	{
		return false;
	}

	for (int size : layers)
	{
		if (size <= 0 || size > 65536) return false;
	}

	NeuralNetwork loaded(layers, linear);

	if (!loaded.setParameters(parameters)) return false;

	*this = loaded;

	if (generation) *generation = storedGeneration;
	if (fitness) *fitness = storedFitness;

	return true;
}
