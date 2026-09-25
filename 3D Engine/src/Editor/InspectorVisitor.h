#pragma once
#include "Properties.h"

#include <set>
#include <string>

// Ставить підпис у ліву колонку, а саме поле розтягує на решту ширини. Власні підписи ImGui
// малює праворуч від поля, і у вузькій панелі інспектора вони просто зрізаються за краєм.
// overridden позначає поле, змінене в екземплярі префаба, як це робить Unity
void inspectorLabel(const char* label, bool overridden = false);

// Колір, яким редактор позначає все, що стосується префабів
extern const float PREFAB_COLOR[4];

// Малює поля компонента в інспекторі редактора та одразу записує зміни назад у компонент
class InspectorVisitor : public PropertyVisitor
{
public:
	void property(const char* name, float& value, float step) override;
	void property(const char* name, int& value) override;
	void property(const char* name, bool& value) override;
	void property(const char* name, Vector3& value) override;
	void property(const char* name, std::string& value) override;

	void color(const char* name, float* channels, int count) override;

	void reference(const char* name, Entity*& value, ReferenceKind kind) override;

	// Поля між цими викликами показуються, але не редагуються
	void beginReadOnly() override;
	void endReadOnly() override;

	// Задає перелік змінених полів екземпляра префаба та початок ключів полів цього компонента
	void setOverrides(const std::set<std::string>* overrides, const std::string& keyPrefix);

	using PropertyVisitor::property;
	using PropertyVisitor::reference;

private:
	// Скільки разів поспіль увімкнено режим лише для читання
	int mReadOnly = 0;

	// Перевіряє, чи поле змінене в екземплярі префаба
	bool isOverridden(const char* name) const;

	const std::set<std::string>* mOverrides = nullptr;
	std::string mKeyPrefix;
};
