#include "DynamicsModel.h"
#include "VehicleController.h"
#include "Entity.h"
#include "Gizmo.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "imgui.h"
#include <map>
#include <memory>
#include <chrono>
#include <iostream>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Прочитана модель разом із часом зміни файлу, з якого її прочитано
struct SharedModel
{
	// Сама модель
	std::unique_ptr<LearnedDynamics> model;
	// Час зміни файлу на момент читання; змінився - модель перечитується
	filesystem::file_time_type stamp;
	// Чи файл удалося прочитати
	bool valid = false;
	// Коли файл перевіряли востаннє; частіше ніж раз на секунду його не перевіряємо
	std::chrono::steady_clock::time_point checked;
	// Чи файл уже перевіряли
	bool everChecked = false;
};

// Прочитані моделі за шляхами файлів; одну модель ділять усі агенти, що на неї посилаються
static std::map<std::string, SharedModel>& sharedModels()
{
	static std::map<std::string, SharedModel> models;
	return models;
}

// Читає модель з файлу ресурсу один раз для всіх, хто її просить; nullptr, якщо файл не прочитано
const LearnedDynamics* DynamicsModel::loadShared(const std::string& path)
{
	if (path.empty()) return nullptr;

	SharedModel& entry = sharedModels()[path];

	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	// Модель питають щокроку, а файл змінюється рідко, тож перевіряємо його раз на секунду
	if (entry.everChecked && now - entry.checked < std::chrono::seconds(1)) return entry.valid ? entry.model.get() : nullptr;

	entry.checked = now;
	entry.everChecked = true;

	std::error_code error;
	filesystem::file_time_type stamp = filesystem::last_write_time(path, error);

	if (error)
	{
		entry.valid = false;
		return nullptr;
	}

	// Тренер перезаписує файл під час гри, тож нова версія підхоплюється без перезапуску
	if (!entry.model || entry.stamp != stamp)
	{
		std::unique_ptr<LearnedDynamics> model(new LearnedDynamics());

		entry.stamp = stamp;
		entry.valid = model->load(path);

		if (entry.valid) entry.model = std::move(model);
		else std::cout << "DynamicsModel: can't load " << path << std::endl;
	}

	return entry.valid ? entry.model.get() : nullptr;
}

// Повертає модель, прочитану з ресурсу, або nullptr, якщо файл не прочитано
const LearnedDynamics* DynamicsModel::getModel()
{
	return loadShared(modelAsset);
}

// Прогнозує шлях автомобіля на steps кроків, якщо поточні дії не зміняться; false, якщо моделі чи автомобіля немає
bool DynamicsModel::predictPath(int steps, std::vector<Vector3>& path)
{
	path.clear();

	const LearnedDynamics* model = getModel();
	VehicleController* vehicle = getOwner()->getComponent<VehicleController>();

	if (!model || !vehicle || !vehicle->getRigidBody()) return false;

	float state[LearnedDynamics::STATE_SIZE];
	float next[LearnedDynamics::STATE_SIZE];
	float action[LearnedDynamics::ACTION_SIZE] = { vehicle->getThrottleAction(), vehicle->getSteeringAction() };
	float x, z, heading;

	vehicle->getDynamicsState(state);
	vehicle->getPlanarPose(x, z, heading);

	// Шлях малюється трохи над землею, на висоті корпусу
	float height = getOwner()->getTransform()->getPosition().y;

	path.push_back(Vector3(x, height, z));

	for (int i = 0; i < steps; i++)
	{
		model->predict(state, action, next);
		LearnedDynamics::integratePose(x, z, heading, next, SimulationSettings::fixedStep);

		for (int k = 0; k < LearnedDynamics::STATE_SIZE; k++) state[k] = next[k];

		path.push_back(Vector3(x, height, z));
	}

	return true;
}

// Малює ламану в просторі сцени поверх кадру
void DynamicsModel::drawPath(const std::vector<Vector3>& path, unsigned int color, float thickness)
{
	ImDrawList* draw = ImGui::GetBackgroundDrawList();

	bool havePrevious = false;
	ImVec2 previous;

	for (const Vector3& point : path)
	{
		float sx, sy;

		// Точка за камерою розриває лінію, щоб не тягнути відрізок через увесь екран
		if (!Gizmo::projectToScreen(point, sx, sy))
		{
			havePrevious = false;
			continue;
		}

		ImVec2 current(sx, sy);

		if (havePrevious) draw->AddLine(previous, current, color, thickness);

		previous = current;
		havePrevious = true;
	}
}

// Малює прогнозований шлях автомобіля
void DynamicsModel::update()
{
	if (!showPrediction) return;

	if (predictPath(predictionSteps, mPath)) drawPath(mPath, IM_COL32(60, 230, 120, 255), 3.0f);
}

// Перелічує власні поля для файлу сцени та інспектора
void DynamicsModel::visitProperties(PropertyVisitor& visitor)
{
	visitor.asset("modelAsset", modelAsset, LearnedDynamics::FILE_EXTENSION);
	visitor.property("showPrediction", showPrediction);
	visitor.property("predictionSteps", predictionSteps);

	if (predictionSteps < 1) predictionSteps = 1;
	if (predictionSteps > 500) predictionSteps = 500;
}
