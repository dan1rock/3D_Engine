#include "InspectorVisitor.h"
#include "Entity.h"
#include "Prefab.h"
#include "EntityManager.h"

#include "imgui.h"

#include <vector>

// Перетворює ім'я поля на підпис, звичний для редактора: forceDamping -> Force Damping
static std::string toLabel(const char* name)
{
	std::string label;

	for (const char* symbol = name; *symbol; symbol++)
	{
		// Велика літера всередині слова означає межу слів
		if (symbol != name && *symbol >= 'A' && *symbol <= 'Z') label += ' ';

		if (symbol == name && *symbol >= 'a' && *symbol <= 'z') label += (char)(*symbol - 'a' + 'A');
		else label += *symbol;
	}

	return label;
}

// Ставить підпис у ліву колонку, а саме поле розтягує на решту ширини
void inspectorLabel(const char* label)
{
	float available = ImGui::GetContentRegionAvail().x;

	// Вужче за третину панелі підпис не стискаємо, інакше від нього нічого не лишиться
	float labelWidth = available * 0.42f;
	if (labelWidth < 80.0f) labelWidth = 80.0f;

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine(labelWidth);
	ImGui::SetNextItemWidth(-1.0f);
}

// Ім'я поля стає і підписом, і прихованим ідентифікатором віджета
static std::string fieldId(const char* name)
{
	return std::string("##") + name;
}

void InspectorVisitor::property(const char* name, float& value, float step)
{
	inspectorLabel(toLabel(name).c_str());
	ImGui::DragFloat(fieldId(name).c_str(), &value, step);
}

void InspectorVisitor::property(const char* name, int& value)
{
	inspectorLabel(toLabel(name).c_str());
	ImGui::DragInt(fieldId(name).c_str(), &value);
}

void InspectorVisitor::property(const char* name, bool& value)
{
	inspectorLabel(toLabel(name).c_str());
	ImGui::Checkbox(fieldId(name).c_str(), &value);
}

void InspectorVisitor::property(const char* name, Vector3& value)
{
	inspectorLabel(toLabel(name).c_str());
	ImGui::DragFloat3(fieldId(name).c_str(), &value.x, 0.05f);
}

void InspectorVisitor::property(const char* name, std::string& value)
{
	char buffer[256];

	strncpy_s(buffer, sizeof(buffer), value.c_str(), _TRUNCATE);

	inspectorLabel(toLabel(name).c_str());
	if (ImGui::InputText(fieldId(name).c_str(), buffer, sizeof(buffer)))
	{
		value = buffer;
	}
}

void InspectorVisitor::color(const char* name, float* channels, int count)
{
	inspectorLabel(toLabel(name).c_str());

	if (count >= 4) ImGui::ColorEdit4(fieldId(name).c_str(), channels);
	else ImGui::ColorEdit3(fieldId(name).c_str(), channels);
}

void InspectorVisitor::reference(const char* name, Entity*& value, ReferenceKind kind)
{
	// У списку показуємо ім'я об'єкта, на який зараз указує поле
	std::string current = value ? value->getName() : "none";

	inspectorLabel(toLabel(name).c_str());

	if (!ImGui::BeginCombo(fieldId(name).c_str(), current.c_str())) return;

	if (ImGui::Selectable("none", value == nullptr)) value = nullptr;

	for (Entity* entity : EntityManager::get()->getEntities())
	{
		// Поле під образ не має пропонувати звичайні об'єкти сцени
		if (kind == ReferenceKind::PrefabOnly && dynamic_cast<Prefab*>(entity) == nullptr) continue;

		// Однакові імена трапляються часто, тому кожен рядок отримує власний ідентифікатор
		ImGui::PushID(entity);

		if (ImGui::Selectable(entity->getName().c_str(), entity == value)) value = entity;

		ImGui::PopID();
	}

	ImGui::EndCombo();
}

// Поля між цими викликами показуються, але не редагуються
void InspectorVisitor::beginReadOnly()
{
	if (mReadOnly++ == 0) ImGui::BeginDisabled();
}

// Поля між цими викликами показуються, але не редагуються
void InspectorVisitor::endReadOnly()
{
	if (--mReadOnly == 0) ImGui::EndDisabled();
}
