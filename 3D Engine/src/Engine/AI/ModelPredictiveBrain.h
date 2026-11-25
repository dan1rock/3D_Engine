#pragma once
#include "AgentBrain.h"
#include "Vector3.h"
#include <string>
#include <vector>

class LearnedDynamics;
class ControllableBody;

// Базовий клас мозку з прогнозуючим керуванням: перебирає поєднання дій із заданих наборів, прогнозує їх навченою моделлю динаміки й виконує найкраще; оцінку кроків прогнозу задає похідний клас
class ModelPredictiveBrain : public AgentBrain
{
public:
	// Шлях до файлу навченої моделі динаміки в ресурсах проєкту
	std::string modelAsset = "Assets\\Networks\\Dynamics.dyn";
	// Рівні кожної дії через кому, набори різних дій розділено крапкою з комою; кандидати - усі їхні поєднання
	std::string actionLevels = "-1, 0, 1";
	// Скільки секунд кандидат тримає дію з набору; далі до кінця горизонту дії підбирає continueAction
	float holdTime = 0.3f;
	// Довжина прогнозу в кроках фізики
	int horizon = 25;
	// Через скільки кроків план складається заново; між ними виконується вже складений
	int replanInterval = 2;
	// Вага штрафу за ймовірність зіткнення, яку передбачає навчена модель зіткнень
	float collisionWeight = 30.0f;
	// Чи малювати вибраний прогнозований шлях
	bool showPlan = true;
	// Чи малювати під вибраним шляхом і решту розглянутих: тонкими лініями, кольором за місцем в оцінці
	bool showCandidates = true;

	// Виконує дію зі складеного плану, а коли настав час - складає новий
	void decide(Agent& agent, const float* observation, float* action) override;
	// Відкидає старий план на початку епізоду
	void onEpisodeBegin(Agent& agent) override;

	// Перелічує власні поля для файлу сцени та інспектора
	void visitProperties(PropertyVisitor& visitor) override;

protected:
	// Повертає довжину прогнозу в кроках для поточного стану
	virtual int getPlanningSteps(const float* state) const;
	// Повертає, скільки кроків кандидат тримає дію з набору
	virtual int getHoldSteps(const float* state, int steps) const;
	// Готується до оцінки: обчислює те, що спільне для всіх кандидатів, наприклад перешкоди чи маршрут
	virtual void beginPlanning(Agent& agent, const float* state, float x, float z, float heading, int candidates, int steps) {}
	// Підбирає дію кандидата після утримання дії з набору; action уже містить дію попереднього кроку
	virtual void continueAction(int candidate, int step, const float* state, float x, float z, float heading, float* action) {}
	// Оцінює крок прогнозу кандидата: стан і поза після кроку та дія на ньому; більше - краще
	virtual float evaluateStep(int candidate, int step, const float* state, const float* action, const float* previousAction, float x, float z, float heading) = 0;
	// Оцінює кінець прогнозу кандидата; більше - краще
	virtual float evaluateEnd(int candidate, const float* state, float x, float z, float heading) { return 0.0f; }

	// Повертає модель динаміки з ресурсу або nullptr
	const LearnedDynamics* getModel() const;
	// Відкидає складений план, щоб наступне рішення склало новий
	void resetPlan();

	// Малює вибраний і розглянуті прогнозовані шляхи
	void update() override;

private:
	// Складає новий план: перебирає поєднання дій із наборів і запам'ятовує найкраще
	void plan(Agent& agent, const LearnedDynamics& model, ControllableBody& body);
	// Розбирає набори рівнів дій, якщо їх змінили
	void parseLevels(int actionSize);

	// Дії найкращої послідовності підряд на кожному кроці
	std::vector<float> mPlan;
	// Номер кроку плану, який виконати наступним
	int mPlanStep = 0;
	// Розмір дій плану
	int mActionSize = 0;
	// Розібрані рівні кожної дії
	std::vector<std::vector<float>> mLevels;
	// Рядок, з якого розібрано рівні
	std::string mParsedLevels;
	// Прогнозований шлях найкращої послідовності для малювання
	std::vector<Vector3> mPlannedPath;
	// Прогнозовані шляхи решти кандидатів останнього планування, від найгіршого до найкращого
	std::vector<std::vector<Vector3>> mCandidatePaths;

	// Послідовності дій усіх кандидатів
	std::vector<float> mSequences;
	// Стани кандидатів на поточному кроці прогнозу
	std::vector<float> mStates;
	// Передбачені наступні стани кандидатів
	std::vector<float> mNextStates;
	// Дії кандидатів на поточному кроці прогнозу
	std::vector<float> mActions;
	// Прогнозовані положення всіх кандидатів на всіх кроках
	std::vector<float> mPositions;
};
