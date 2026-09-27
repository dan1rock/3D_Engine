#pragma once
#include "EditorCamera.h"
#include "Gizmo.h"
#include "EditorGrid.h"
#include "SelectionOutline.h"
#include "SceneHistory.h"
#include "Vector3.h"
#include <string>
#include <vector>
#include <set>
#include <unordered_set>

class Entity;
class Component;
class Transform;
class Renderer;
class Material;
class SwapChain;

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
	// Малює допоміжну геометрію редактора, як-от сітку й обведення вибраного, поверх готового кадру,
	// але під інтерфейсом; width і height - розмір вікна
	void renderOverlay(SwapChain* swapChain, unsigned int width, unsigned int height);

	// Перевіряє, чи сцена зараз програється, а не редагується
	bool isPlaying() const;
	// Перевіряє, чи гру поставлено на паузу
	bool isPaused() const;
	// Перевіряє, чи цього кадру сцена оновлюється: гра йде без паузи, або редактор сховано
	bool isSimulating() const;
	// Перевіряє, чи показано інтерфейс редактора
	bool isEnabled() const;
	// Перевіряє, чи редактор зараз у режимі редагування префаба
	bool isPrefabMode() const;

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
	// Приймає перетягнутий об'єкт на рядок дерева: над ним, під ним чи всередину
	void drawDropTarget(Entity* target);
	// Перевіряє, чи можна зробити об'єкт дочірнім для вказаного батька (nullptr — корінь)
	bool canDrop(Entity* dragged, Entity* newParent) const;
	// Перевіряє, чи можна покласти новий екземпляр префаба під вказаного батька
	bool canDropPrefab(Entity* newParent) const;
	// Виконує відкладене перетягування, коли дерево вже намальоване
	void applyDrop();

	// Малює панель префаба у вибраного екземпляра: застосувати, скасувати зміни, розірвати зв'язок
	void drawPrefabBar();
	// Створює екземпляр префаба, відпущеного над самою сценою, у точці під курсором
	void dropPrefabIntoScene();
	// Перевіряє, чи поле вибраного об'єкта змінене відносно префаба
	bool isOverridden(const std::string& key) const;

	// Відкриває префаб в ізольованій сцені, де є лише він сам
	void openPrefab(const std::string& path);
	// Повертає сцену, з якої відкривали префаб; save спершу записує зміни у файл префаба
	void closePrefab(bool save);
	// Просить закрити префаб: із незбереженими змінами спершу питає, чи їх зберегти
	void requestClosePrefab();
	// Перевіряє, чи вміст префаба в редакторі відрізняється від файлу
	bool isPrefabDirty();
	// Малює смугу режиму префаба зверху та вікно з питанням про збереження
	void drawPrefabModeBar();
	// Виконує відкладені відкриття та закриття префаба, коли всі панелі вже намальовано
	void applyPrefabModeRequests();
	// У режимі префаба робить новий кореневий об'єкт дочірнім для кореня префаба, бо в префабі
	// корінь лише один і все інше мусить лежати під ним
	void adoptIntoPrefab(Entity* entity);
	// Малює поля трансформації об'єкта
	void drawTransform(Entity* entity);
	// Малює меню створення нового об'єкта
	void drawCreateMenu();
	// Малює меню додавання компонента до вибраного об'єкта
	void drawAddComponentMenu(Entity* entity);
	// Малює вибір меша та матеріалів для рендер-компонента
	void drawRendererAssets(Renderer* renderer);
	// Малює поля матеріалу вказаного слота рендер-компонента
	void drawMaterial(Renderer* renderer, int slot);
	// Повертає матеріал слота, яким користується лише цей рендер-компонент, за потреби зробивши копію
	Material* ownMaterial(Renderer* renderer, int slot);

	// Помічає, що сцену замінили, і забуває вибір, якщо вибраний об'єкт знищено
	void trackSceneLoads();
	// Просить відкрити сцену з файлу або, з createNew, створити нову; незбережені зміни спершу
	// пропонується зберегти
	void requestScene(const std::string& path, bool createNew);
	// Виконує відкладене відкриття чи створення сцени, коли всі панелі вже намальовано
	void applySceneRequests();
	// Малює питання про збереження сцени перед переходом до іншої
	void drawScenePrompt();
	// Перевіряє, чи сцена змінилася від останнього відкриття чи збереження
	bool isSceneDirty();
	// Запам'ятовує поточний стан сцени як збережений
	void markSceneSaved();
	// Зберігає відкриту сцену у її файл
	void saveScene();

	// Ставить гру на паузу або знімає з неї; на паузі сцену знову можна редагувати
	void setPaused(bool paused);

	// Переходить у режим гри, зберігши стан сцени
	void play();
	// Повертається до редагування, відновивши збережений стан сцени
	void stop();

	// Обробляє вибір об'єкта мишею та малює маніпулятор
	void updateSelection();

	// Наводить камеру редактора на вибраний об'єкт
	void focusSelected();

	// Знищує вибрані об'єкти
	void deleteSelected();

	// Прибирає з вибору знищені об'єкти і узгоджує перелік вибраних з активним об'єктом
	void syncSelection();
	// Оновлює множину вибраних після зміни переліку
	void updateSelectionSet();
	// Перевіряє, чи об'єкт вибрано
	bool isSelected(Entity* entity) const;
	// Лишає вибраним лише вказаний об'єкт; nullptr знімає вибір
	void selectOnly(Entity* entity);
	// Додає об'єкт до вибору або прибирає з нього
	void toggleSelected(Entity* entity);
	// Вибирає рядки дерева від опорного до вказаного, як Shift у Unity
	void selectRange(Entity* entity);
	// Обробляє клік по рядку дерева з урахуванням Ctrl та Shift
	void clickHierarchyRow(Entity* entity);
	// Повертає вибрані об'єкти без тих, чий предок теж вибраний, у порядку дерева
	std::vector<Entity*> topLevelSelection() const;
	// Вибирає всі об'єкти сцени
	void selectAll();

	// Обробляє поєднання клавіш з Ctrl
	void handleShortcuts();

	// Записує поточний вибір як незмінні номери об'єктів
	void captureSelection(std::vector<int>& selection, int& active) const;
	// Вибирає об'єкти за їхніми номерами
	void restoreSelection(const std::vector<int>& selection, int active);
	// Записує крок скасування, якщо сцена змінилася від попереднього запису
	void recordUndo();
	// Починає історію від поточного стану сцени
	void resetUndo();
	// Скасовує останню зміну
	void undo();
	// Повертає скасовану зміну
	void redo();

	// Скопійовані об'єкти: їхній опис, розмір кожного піддерева та самі джерела, поки вони живі
	struct Clipboard
	{
		std::string data;
		std::vector<int> subtreeSizes;
		std::vector<Entity*> sources;
		unsigned int generation = 0;
	};

	// Записує вибрані об'єкти в буфер
	void copySelection(Clipboard& clipboard) const;
	// Створює копії об'єктів з буфера; поруч із джерелами, якщо вони ще в цій самій сцені
	void pasteClipboard(const Clipboard& clipboard);
	// Створює копії вибраних об'єктів поруч з ними
	void duplicateSelection();

	Entity* mSelected = nullptr;
	// Усі вибрані об'єкти; mSelected серед них активний, його показує інспектор і на ньому стоїть маніпулятор
	std::vector<Entity*> mSelection;
	// Ті самі об'єкти множиною, щоб перевірка вибору не обходила весь перелік
	std::unordered_set<Entity*> mSelectionSet;
	// Рядок, від якого Shift вибирає діапазон у дереві
	Entity* mSelectionAnchor = nullptr;
	// Рядки дерева в порядку показу: минулий кадр для діапазону Shift і поточний, що саме збирається
	std::vector<Entity*> mHierarchyRows;
	std::vector<Entity*> mHierarchyRowsBuilding;
	// Вибраний рядок, клік по якому без перетягування лишить вибраним тільки його
	Entity* mPendingSingleSelect = nullptr;

	// Історія змін для скасування
	SceneHistory mHistory;
	// Вибір перед зміною, яку ще не записали: його поверне скасування цієї зміни
	std::vector<int> mUndoSelection;
	int mUndoActive = -1;
	// Скільки ще кадрів перевіряти сцену на зміни після дії користувача
	int mUndoCheckFrames = 0;
	// Чи було якесь поле активним минулого кадру: коли його відпускають, правку завершено
	bool mWasItemActive = false;

	// Буфер копіювання та номер покоління сцени: після перебудови сцени старі вказівники вже не джерела
	Clipboard mClipboard;
	unsigned int mSceneGeneration = 0;

	// Куди відпустили перетягнутий об'єкт відносно рядка під курсором
	enum class DropZone
	{
		Before,
		Inside,
		After,
		Root
	};

	// Перетягування запам'ятовується і виконується після малювання дерева: зміна батьків
	// посеред обходу списків дітей зламала б сам обхід
	struct PendingDrop
	{
		Entity* dragged = nullptr;
		Entity* target = nullptr;
		DropZone zone = DropZone::Root;
		// Шлях префаба, якщо перетягнули не об'єкт, а префаб з панелі ресурсів
		std::string prefab;
	};

	PendingDrop mPendingDrop;

	// Для вибраного об'єкта: корінь його екземпляра префаба, шлях об'єкта в ньому та змінені поля.
	// Рахуються раз на кадр, бо ними користуються і панель префаба, і підписи полів
	Entity* mInstanceRoot = nullptr;
	std::string mInstancePath;
	std::set<std::string> mOverrides;
	// Початок ключів полів компонента, що саме малюється в інспекторі
	std::string mComponentKey;

	// Батько, який треба розгорнути в дереві, щоб щойно покладений у нього об'єкт було видно
	Entity* mExpandEntity = nullptr;

	// Режим префаба: шлях до файлу (порожній поза режимом) та корінь префаба в ізольованій сцені
	std::string mPrefabModePath;
	Entity* mPrefabRoot = nullptr;
	// Сцена, до якої треба повернутися, вибраний у ній об'єкт і положення камери
	std::string mPrefabModeScene;
	int mPrefabModeSelected = -1;
	EditorCamera::View mPrefabModeView;

	// Префаб, двічі клікнутий у панелі ресурсів: відкривається, лише коли кнопку відпустили без
	// перетягування, інакше другий клік перед перетягуванням відкривав би префаб
	std::string mPendingPrefabOpen;
	// Відкладені дії: відкрити префаб, закрити поточний (1 — без збереження, 2 — зі збереженням)
	std::string mOpenPrefabRequest;
	int mClosePrefabRequest = 0;
	// Показати питання про збереження та префаб, який відкрити після закриття поточного
	bool mShowSavePrompt = false;
	std::string mAfterClose;

	bool mEnabled = true;
	bool mPlaying = false;
	// Гра на паузі: компоненти й фізика стоять, а сценою знову керує редактор
	bool mPaused = false;
	// Положення камери редактора до запуску гри; після зупинки вона повертається туди
	EditorCamera::View mPlayView;

	EditorCamera mCamera;

	// Маніпулятор вибраного об'єкта та вибір мишею у вікні сцени
	Gizmo mGizmo;
	// Сітка площини XZ, яку показує режим редагування префаба
	EditorGrid mGrid;
	// Обведення вибраного об'єкта у вікні сцени
	SelectionOutline mOutline;

	// Шляхи до знайдених ресурсів, які пропонує інспектор
	std::vector<std::wstring> mMeshPaths;
	std::vector<std::wstring> mTexturePaths;
	std::vector<std::string> mMeshNames;
	std::vector<std::string> mTextureNames;

	// Стан сцени на момент відкриття чи збереження, з яким порівнюються зміни
	std::string mSavedScene;
	// Позначка незбережених змін для панелі. Сцена записується для порівняння не щокадру, а
	// раз на кілька кадрів, бо у великій сцені це помітна робота
	bool mSceneDirty = false;
	int mDirtyCheckFrame = 0;
	// Скільки завантажень сцени редактор уже бачив
	unsigned int mSeenLoadCount = 0;

	// Відкладений перехід: шлях сцени, яку відкрити, або створення нової
	std::string mSceneRequest;
	bool mNewSceneRequest = false;
	// Рішення щодо незбережених змін: 0 - ще не прийняте, 1 - не зберігати, 2 - зберегти, -1 - скасувати
	int mSceneDecision = 0;
	bool mShowScenePrompt = false;

	// Сцена, відкрита до запуску гри: гра могла перемкнути сцену, а зупинка повертає цю
	std::string mPlayScenePath;
	// Уся сцена, записана перед запуском гри тим самим серіалізатором, що й файл сцени
	std::string mPlayScene;
	// Об'єкти у порядку знімка: за ним вибраний об'єкт знаходить свою відновлену копію.
	// Вказівники лише порівнюються, бо гра могла будь-який з них знищити
	std::vector<Entity*> mPlayOrder;
	// Номер об'єкта, вибраного в момент запуску: до нього вибір повертається, якщо гра знищила
	// вибраний об'єкт або вибрано щось, що вона сама створила
	int mPlaySelectedIndex = -1;

	// Буфер для поля імені об'єкта
	char mNameBuffer[128] = {};
};
