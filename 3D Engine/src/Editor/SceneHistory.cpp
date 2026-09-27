#include "SceneHistory.h"

SceneHistory::SceneHistory()
{
	mSteps.resize(CAPACITY);
}

// Починає історію від поточного стану сцени, забуваючи всі кроки
void SceneHistory::reset()
{
	for (Step& step : mSteps)
	{
		step = Step();
	}

	mStart = 0;
	mCount = 0;
	mPosition = 0;

	// Номери матеріалів попередньої сцени більше нічого не означають
	mRegistry = MaterialRegistry();

	SceneSerializer::captureRecords(mCurrent, mRegistry);
}

// Записує крок, якщо сцена змінилася від попереднього запису; повертає, чи крок з'явився
bool SceneHistory::record(const std::vector<int>& selectionBefore, int activeBefore, const std::vector<int>& selectionAfter, int activeAfter)
{
	SceneRecords now;
	SceneSerializer::captureRecords(now, mRegistry);

	Step step;

	// Змінені та створені об'єкти
	for (const auto& entry : now.entities)
	{
		auto old = mCurrent.entities.find(entry.first);

		if (old == mCurrent.entities.end()) step.entities.push_back({ entry.first, std::string(), entry.second });
		else if (old->second != entry.second) step.entities.push_back({ entry.first, old->second, entry.second });
	}

	// Знищені об'єкти
	for (const auto& entry : mCurrent.entities)
	{
		if (now.entities.count(entry.first) == 0) step.entities.push_back({ entry.first, entry.second, std::string() });
	}

	// Значення матеріалів; новий матеріал до кроку ще нікому не належав, тож відновлювати в ньому нічого
	for (const auto& entry : now.materials)
	{
		auto old = mCurrent.materials.find(entry.first);

		if (old == mCurrent.materials.end()) step.materials.push_back({ entry.first, std::string(), entry.second });
		else if (old->second != entry.second) step.materials.push_back({ entry.first, old->second, entry.second });
	}

	if (now.order != mCurrent.order)
	{
		step.orderBefore = mCurrent.order;
		step.orderAfter = now.order;
	}

	mCurrent = std::move(now);

	bool changed = !step.entities.empty() || !step.orderBefore.empty();

	for (const Change& change : step.materials)
	{
		if (!change.before.empty()) changed = true;
	}

	if (!changed) return false;

	step.selectionBefore = selectionBefore;
	step.activeBefore = activeBefore;
	step.selectionAfter = selectionAfter;
	step.activeAfter = activeAfter;

	// Скасовані кроки після поточного зникають, як і в будь-якому редакторі
	mCount = mPosition;

	// Коло заповнене: найстаріший крок звільняє місце
	if (mCount == CAPACITY)
	{
		mStart = (mStart + 1) % CAPACITY;
		mCount--;
		mPosition--;
	}

	at(mCount) = std::move(step);

	mCount++;
	mPosition++;

	return true;
}

// Скасовує останній крок і віддає вибір, що був до нього; false, якщо скасовувати нічого
bool SceneHistory::undo(std::vector<int>& selection, int& active)
{
	if (mPosition == 0) return false;

	Step& step = at(mPosition - 1);

	apply(step, false);

	mPosition--;

	selection = step.selectionBefore;
	active = step.activeBefore;

	return true;
}

// Повертає скасований крок і віддає вибір після нього; false, якщо повертати нічого
bool SceneHistory::redo(std::vector<int>& selection, int& active)
{
	if (mPosition >= mCount) return false;

	Step& step = at(mPosition);

	apply(step, true);

	mPosition++;

	selection = step.selectionAfter;
	active = step.activeAfter;

	return true;
}

// Кількість кроків, які зараз можна скасувати
int SceneHistory::getUndoCount() const
{
	return mPosition;
}

// Кількість кроків, які зараз можна повернути
int SceneHistory::getRedoCount() const
{
	return mCount - mPosition;
}

// Приводить сцену до стану до кроку або після нього
void SceneHistory::apply(const Step& step, bool forward)
{
	std::vector<RecordChange> entities;
	std::vector<std::pair<int, std::string>> materials;

	for (const Change& change : step.entities)
	{
		RecordChange record;
		record.id = change.id;
		record.from = forward ? change.before : change.after;
		record.to = forward ? change.after : change.before;

		entities.push_back(record);
	}

	for (const Change& change : step.materials)
	{
		const std::string& value = forward ? change.after : change.before;

		if (!value.empty()) materials.push_back(std::make_pair(change.id, value));
	}

	SceneSerializer::applyRecords(entities, materials, forward ? step.orderAfter : step.orderBefore, mRegistry);

	// Поточним стає свіжий запис, щоб дрібні відмінності перезапису не стали окремим кроком
	SceneSerializer::captureRecords(mCurrent, mRegistry);
}

// Повертає крок за його номером від найстарішого
SceneHistory::Step& SceneHistory::at(int index)
{
	return mSteps[(mStart + index) % CAPACITY];
}
