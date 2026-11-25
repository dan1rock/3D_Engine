#pragma once
#include "Component.h"
#include "LearnedDynamics.h"
#include "Vector3.h"
#include <string>
#include <vector>

class ControllableBody;

// Сервіс навченої моделі динаміки: читає модель з ресурсу, прогнозує рух керованого об'єкта на своєму об'єкті й показує прогнозований шлях
class DynamicsModel : public Component
{
public:
	COMPONENT_TYPE(DynamicsModel)

	// Шлях до файлу навченої моделі динаміки в ресурсах проєкту
	std::string modelAsset = "Assets\\Networks\\Vehicle Dynamics.dyn";
	// Чи малювати прогнозований шлях за поточних дій
	bool showPrediction = true;
	// На скільки кроків фізики вперед прогнозувати шлях
	int predictionSteps = 50;

	// Повертає модель, прочитану з ресурсу, або nullptr, якщо файл не прочитано
	const LearnedDynamics* getModel();
	// Прогнозує шлях керованого об'єкта на steps кроків, якщо поточні дії не зміняться; false, якщо моделі чи об'єкта немає або вони не відповідають одне одному
	bool predictPath(int steps, std::vector<Vector3>& path);

	// Малює ламану в просторі сцени поверх кадру
	static void drawPath(const std::vector<Vector3>& path, unsigned int color, float thickness);
	// Пускає промені огляду моделі зіткнень по колу навколо керованого об'єкта й записує відстані, поділені на дальність
	static void scanSurroundings(ControllableBody* body, const LearnedDynamics& model, float* out);
	// Перевіряє, чи модель навчено для керованого об'єкта саме таких розмірів стану й дій
	static bool isCompatible(const LearnedDynamics& model, const ControllableBody& body);
	// Читає модель з файлу ресурсу один раз для всіх, хто її просить; nullptr, якщо файл не прочитано
	static const LearnedDynamics* loadShared(const std::string& path);

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Створює копію компонента для нового об'єкта
	DynamicsModel* instantiate() const override
	{
		return new DynamicsModel(*this);
	}

private:
	// Малює прогнозований шлях і ймовірність зіткнення
	void update() override;

	// Останній прогнозований шлях
	std::vector<Vector3> mPath;
};
