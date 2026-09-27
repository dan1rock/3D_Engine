#pragma once
#include "SceneSerializer.h"
#include <string>
#include <vector>

// Історія змін сцени для скасування: крок пам'ятає лише змінені об'єкти й матеріали, тож скасування правлять тільки їх
class SceneHistory
{
public:
	// Скільки кроків пам'ятає історія; новий крок понад це число перезаписує найстаріший
	static const int CAPACITY = 100;

	SceneHistory();

	// Починає історію від поточного стану сцени, забуваючи всі кроки
	void reset();
	// Записує крок, якщо сцена змінилася від попереднього запису; повертає, чи крок з'явився
	bool record(const std::vector<int>& selectionBefore, int activeBefore, const std::vector<int>& selectionAfter, int activeAfter);
	// Скасовує останній крок і віддає вибір, що був до нього; false, якщо скасовувати нічого
	bool undo(std::vector<int>& selection, int& active);
	// Повертає скасований крок і віддає вибір після нього; false, якщо повертати нічого
	bool redo(std::vector<int>& selection, int& active);

	// Кількість кроків, які зараз можна скасувати
	int getUndoCount() const;
	// Кількість кроків, які зараз можна повернути
	int getRedoCount() const;

private:
	// Зміна одного об'єкта чи матеріалу: запис до й після, порожній означає, що його не було
	struct Change
	{
		int id = 0;
		std::string before;
		std::string after;
	};

	// Один крок: змінені об'єкти й матеріали, порядок дерева та вибір до і після
	struct Step
	{
		std::vector<Change> entities;
		std::vector<Change> materials;
		std::vector<int> orderBefore;
		std::vector<int> orderAfter;
		std::vector<int> selectionBefore;
		std::vector<int> selectionAfter;
		int activeBefore = -1;
		int activeAfter = -1;
	};

	// Приводить сцену до стану до кроку або після нього
	void apply(const Step& step, bool forward);
	// Повертає крок за його номером від найстарішого
	Step& at(int index);

	// Кроки по колу: mStart - найстаріший, mCount - скільки записано, mPosition - скільки з них зараз діють
	std::vector<Step> mSteps;
	int mStart = 0;
	int mCount = 0;
	int mPosition = 0;

	// Стан сцени після останнього кроку, з яким порівнюється наступний
	SceneRecords mCurrent;
	// Незмінні номери матеріалів, на які посилаються записи
	MaterialRegistry mRegistry;
};
