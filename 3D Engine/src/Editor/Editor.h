#pragma once
#include "EditorCamera.h"
#include "Gizmo.h"
#include "Vector3.h"
#include <string>
#include <vector>

class Entity;
class Component;
class Transform;
class Renderer;
class Material;

// Редактор сцени: показує дерево об'єктів, інспектор вибраного об'єкта та керує режимом гри
class Editor
{
public:
	// Повертає єдиний екземпляр редактора (синглтон)
	static Editor* get();

	// Збирає списки доступних ресурсів для випадних списків інспектора
	void init();

	// Малює інтерфейс редактора та оновлює його камеру
	void update();

	// Перевіряє, чи сцена зараз програється, а не редагується
	bool isPlaying() const;
	// Перевіряє, чи показано інтерфейс редактора
	bool isEnabled() const;

	// Вмикає або вимикає інтерфейс редактора
	void setEnabled(bool enabled);

	// Повертає камеру редактора
	EditorCamera& getCamera();

private:
	Editor();

	// Малює верхню панель з кнопками режиму гри та збереження сцени
	void drawToolbar();
	// Малює дерево об'єктів сцени
	void drawHierarchy();
	// Малює інспектор вибраного об'єкта
	void drawInspector();
	// Малює список ресурсів проєкту
	void drawAssets();

	// Малює один вузол дерева разом з його дочірніми об'єктами
	void drawEntityNode(Entity* entity);
	// Малює поля трансформації об'єкта
	void drawTransform(Entity* entity);
	// Малює меню створення нового об'єкта
	void drawCreateMenu();
	// Малює меню додавання компонента до вибраного об'єкта
	void drawAddComponentMenu(Entity* entity);
	// Малює вибір меша та матеріалів для рендер-компонента
	void drawRendererAssets(Renderer* renderer);
	// Малює поля матеріалу
	void drawMaterial(Material* material, int slot);

	// Переходить у режим гри, зберігши стан сцени
	void play();
	// Повертається до редагування, відновивши збережений стан сцени
	void stop();

	// Обробляє вибір об'єкта мишею та малює маніпулятор
	void updateSelection();

	// Наводить камеру редактора на вибраний об'єкт
	void focusSelected();

	// Знищує вибраний об'єкт
	void deleteSelected();

	Entity* mSelected = nullptr;

	bool mEnabled = true;
	bool mPlaying = false;

	EditorCamera mCamera;

	// Маніпулятор вибраного об'єкта та вибір мишею у вікні сцени
	Gizmo mGizmo;

	// Шляхи до знайдених ресурсів, які пропонує інспектор
	std::vector<std::wstring> mMeshPaths;
	std::vector<std::wstring> mTexturePaths;
	std::vector<std::string> mMeshNames;
	std::vector<std::string> mTextureNames;

	// Знімок одного об'єкта, зроблений перед запуском гри
	struct EntitySnapshot
	{
		Entity* entity = nullptr;
		Vector3 position;
		Vector3 rotation;
		Vector3 scale;
		bool active = true;
		bool hasParent = false;
	};

	// Стан сцени, збережений перед запуском гри
	std::vector<EntitySnapshot> mSnapshot;

	// Буфер для поля імені об'єкта
	char mNameBuffer[128] = {};
};
