#include "InspectorVisitor.h"
#include "Entity.h"
#include "Prefab.h"
#include "EntityManager.h"
#include "PrefabLibrary.h"

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

// Колір, яким редактор позначає все, що стосується префабів
const float PREFAB_COLOR[4] = { 0.45f, 0.72f, 1.0f, 1.0f };

// Ставить підпис у ліву колонку, а саме поле розтягує на решту ширини
void inspectorLabel(const char* label, bool overridden)
{
	float available = ImGui::GetContentRegionAvail().x;

	// Змінене в екземплярі поле позначається смужкою ліворуч і кольором підпису, як в Unity
	if (overridden)
	{
		ImVec2 start = ImGui::GetCursorScreenPos();
		float height = ImGui::GetFrameHeight();

		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(start.x - 6.0f, start.y), ImVec2(start.x - 3.0f, start.y + height),
			ImGui::GetColorU32(ImVec4(PREFAB_COLOR[0], PREFAB_COLOR[1], PREFAB_COLOR[2], PREFAB_COLOR[3])));
	}

	// Вужче за третину панелі підпис не стискаємо, інакше від нього нічого не лишиться
	float labelWidth = available * 0.42f;
	if (labelWidth < 80.0f) labelWidth = 80.0f;

	ImGui::AlignTextToFramePadding();

	if (overridden) ImGui::TextColored(ImVec4(PREFAB_COLOR[0], PREFAB_COLOR[1], PREFAB_COLOR[2], PREFAB_COLOR[3]), "%s", label);
	else ImGui::TextUnformatted(label);

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
	inspectorLabel(toLabel(name).c_str(), isOverridden(name));
	ImGui::DragFloat(fieldId(name).c_str(), &value, step);
}

void InspectorVisitor::property(const char* name, int& value)
{
	inspectorLabel(toLabel(name).c_str(), isOverridden(name));
	ImGui::DragInt(fieldId(name).c_str(), &value);
}

void InspectorVisitor::property(const char* name, bool& value)
{
	inspectorLabel(toLabel(name).c_str(), isOverridden(name));
	ImGui::Checkbox(fieldId(name).c_str(), &value);
}

void InspectorVisitor::property(const char* name, Vector3& value)
{
	inspectorLabel(toLabel(name).c_str(), isOverridden(name));
	ImGui::DragFloat3(fieldId(name).c_str(), &value.x, 0.05f);
}

void InspectorVisitor::property(const char* name, std::string& value)
{
	char buffer[256];

	strncpy_s(buffer, sizeof(buffer), value.c_str(), _TRUNCATE);

	inspectorLabel(toLabel(name).c_str(), isOverridden(name));
	if (ImGui::InputText(fieldId(name).c_str(), buffer, sizeof(buffer)))
	{
		value = buffer;
	}
}

void InspectorVisitor::color(const char* name, float* channels, int count)
{
	inspectorLabel(toLabel(name).c_str(), isOverridden(name));

	if (count >= 4) ImGui::ColorEdit4(fieldId(name).c_str(), channels);
	else ImGui::ColorEdit3(fieldId(name).c_str(), channels);
}

void InspectorVisitor::reference(const char* name, Entity*& value, ReferenceKind kind)
{
	// У списку показуємо ім'я об'єкта, на який зараз указує поле; префаб-файл підписується окремо
	std::string assetPath = PrefabLibrary::get()->getTemplatePath(value);
	std::string current = !assetPath.empty() ? PrefabLibrary::getName(assetPath) + " (prefab asset)" : value ? value->getName() : "none";

	inspectorLabel(toLabel(name).c_str(), isOverridden(name));

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

	// Префаби-файли: поле отримує прихований образ, з якого гра створюватиме копії
	if (kind == ReferenceKind::PrefabOnly)
	{
		const std::vector<std::string>& paths = PrefabLibrary::get()->getPaths();

		if (!paths.empty()) ImGui::Separator();

		for (const std::string& path : paths)
		{
			ImGui::PushID(path.c_str());

			std::string label = PrefabLibrary::getName(path) + " (prefab asset)";

			if (ImGui::Selectable(label.c_str(), path == assetPath))
			{
				value = PrefabLibrary::get()->getTemplate(path);
			}

			ImGui::PopID();
		}
	}

	ImGui::EndCombo();
}

// Задає перелік змінених полів екземпляра префаба та початок ключів полів цього компонента
void InspectorVisitor::setOverrides(const std::set<std::string>* overrides, const std::string& keyPrefix)
{
	mOverrides = overrides;
	mKeyPrefix = keyPrefix;
}

// Перевіряє, чи поле змінене в екземплярі префаба
bool InspectorVisitor::isOverridden(const char* name) const
{
	return mOverrides != nullptr && mOverrides->count(mKeyPrefix + name) > 0;
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
