#pragma once
#include "Properties.h"

// Ставить підпис у ліву колонку, а саме поле розтягує на решту ширини. Власні підписи ImGui
// малює праворуч від поля, і у вузькій панелі інспектора вони просто зрізаються за краєм
void inspectorLabel(const char* label);

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

	using PropertyVisitor::property;
	using PropertyVisitor::reference;

private:
	// Скільки разів поспіль увімкнено режим лише для читання
	int mReadOnly = 0;
};
