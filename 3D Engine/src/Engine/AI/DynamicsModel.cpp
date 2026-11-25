#include "DynamicsModel.h"
#include "ControllableBody.h"
#include "Entity.h"
#include "Gizmo.h"
#include "RigidBody.h"
#include "Physics.h"
#include "SimulationSettings.h"
#include "Properties.h"
#include "imgui.h"
#include <map>
#include <algorithm>
#include <cmath>
#include <cstdio>
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

// Прогнозує шлях керованого об'єкта на steps кроків, якщо поточні дії не зміняться; false, якщо моделі чи об'єкта немає або вони не відповідають одне одному
bool DynamicsModel::predictPath(int steps, std::vector<Vector3>& path)
{
	path.clear();

	const LearnedDynamics* model = getModel();
	ControllableBody* body = getOwner()->getComponent<ControllableBody>();

	if (!model || !body || !body->getBody() || !isCompatible(*model, *body)) return false;

	std::vector<float> state(model->getStateSize());
	std::vector<float> next(model->getStateSize());
	std::vector<float> action(model->getActionSize());
	float x, z, heading;

	body->getState(state.data());
	body->getActions(action.data());
	body->getPlanarPose(x, z, heading);

	// Шлях малюється трохи над землею, на висоті корпусу
	float height = getOwner()->getTransform()->getPosition().y;

	path.push_back(Vector3(x, height, z));

	for (int i = 0; i < steps; i++)
	{
		model->predict(state.data(), action.data(), next.data());
		LearnedDynamics::integratePose(x, z, heading, next.data(), SimulationSettings::fixedStep);

		state.swap(next);

		path.push_back(Vector3(x, height, z));
	}

	return true;
}

// Перевіряє, чи модель навчено для керованого об'єкта саме таких розмірів стану й дій
bool DynamicsModel::isCompatible(const LearnedDynamics& model, const ControllableBody& body)
{
	return model.getStateSize() == body.getStateSize() && model.getActionSize() == body.getActionSize();
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

// Пускає промені огляду моделі зіткнень по колу навколо керованого об'єкта й записує відстані, поділені на дальність
void DynamicsModel::scanSurroundings(ControllableBody* controllable, const LearnedDynamics& model, float* out)
{
	int rays = model.getCollisionRays();
	float range = model.getCollisionRange();
	RigidBody* body = controllable->getBody();

	for (int i = 0; i < rays; i++) out[i] = 1.0f;

	if (!body) return;

	Vector3 position, right, up, forward;
	body->getPose(position, right, up, forward);

	// Промені розходяться по колу з центру корпусу: удар можливий і боком, і задом
	Vector3 origin = position + up * 0.3f;

	for (int i = 0; i < rays; i++)
	{
		float angle = 6.2831853f * (float)i / (float)rays;
		Vector3 direction = forward * std::cos(angle) + right * std::sin(angle);

		RaycastHit hit;

		if (Physics::raycast(origin, direction, range, hit, body))
		{
			out[i] = std::min(hit.distance, range) / range;
		}
	}
}

// Малює прогнозований шлях; колір від зеленого до червоного показує навчену ймовірність зіткнення
void DynamicsModel::update()
{
	if (!showPrediction) return;

	if (!predictPath(predictionSteps, mPath)) return;

	const LearnedDynamics* model = getModel();
	ControllableBody* body = getOwner()->getComponent<ControllableBody>();

	float risk = 0.0f;
	bool hasRisk = model && body && model->hasCollisionModel();

	if (hasRisk)
	{
		std::vector<float> scan(model->getCollisionRays());
		std::vector<float> state(model->getStateSize());
		std::vector<float> action(model->getActionSize());

		scanSurroundings(body, *model, scan.data());
		body->getState(state.data());
		body->getActions(action.data());
		risk = model->predictCollision(scan.data(), state.data(), action.data());
	}

	int red = (int)(60 + 195 * risk);
	int green = (int)(230 - 170 * risk);
	drawPath(mPath, IM_COL32(red, green, 60, 255), 3.0f);

	// Біля об'єкта підписано ймовірність зіткнення за поточних дій
	if (hasRisk && !mPath.empty())
	{
		float sx, sy;

		if (Gizmo::projectToScreen(mPath[0], sx, sy))
		{
			char text[32];
			snprintf(text, sizeof(text), "risk %.0f%%", risk * 100.0f);
			ImGui::GetBackgroundDrawList()->AddText(ImVec2(sx + 12.0f, sy - 24.0f), IM_COL32(red, green, 60, 255), text);
		}
	}
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
