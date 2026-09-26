#pragma once
#include <string>
#include <vector>

// Керує сценами проєкту. Кожна сцена - це файл .scene у теці Assets\Scenes, записаний
// серіалізатором сцени; компоненти перемикають сцени за ім'ям, шляхом або номером
class SceneManager
{
public:
	SceneManager();
	~SceneManager();

	// Повертає єдиний екземпляр менеджера сцен (синглтон)
	static SceneManager* get();

	// Тека, у якій лежать файли сцен
	static const char* FOLDER;
	// Розширення файлів сцен
	static const char* EXTENSION;

	// Перечитує перелік файлів сцен у теці
	void refresh();
	// Повертає шляхи всіх сцен проєкту, упорядковані за ім'ям
	const std::vector<std::string>& getScenePaths() const;
	// Повертає ім'я сцени без теки й розширення
	static std::string getSceneName(const std::string& path);
	// Шукає сцену за ім'ям або шляхом; повертає її шлях або порожній рядок, якщо такої немає
	std::string findScene(const std::string& nameOrPath);

	// Запитує завантаження сцени за ім'ям або шляхом. Сцена завантажується наприкінці кадру,
	// а об'єкти з dontDestroyOnLoad переживають зміну сцени. Повертає false, якщо сцени немає
	bool loadScene(const std::string& nameOrPath);
	// Запитує завантаження сцени за її номером у переліку getScenePaths
	bool loadScene(int index);

	// Одразу замінює сцену вмістом файлу, прибравши й об'єкти з dontDestroyOnLoad, як це робить
	// відкриття сцени в редакторі. Повертає false, якщо файл не вдалося прочитати
	bool openScene(const std::string& path);
	// Створює файл нової сцени зі стандартним вмістом (небо, світло, камера) та одразу відкриває
	// її. Ім'я доповнюється номером, якщо таке вже є. Повертає шлях або порожній рядок
	std::string createScene(const std::string& name);
	// Зберігає поточну сцену у файл відкритої сцени
	bool saveScene();

	// Відкриває першу сцену проєкту, а якщо сцен ще немає - створює сцену за замовчуванням
	void loadStartupScene();

	// Повертає шлях відкритої сцени
	const std::string& getActiveScenePath() const;
	// Повертає ім'я відкритої сцени
	std::string getActiveSceneName() const;
	// Повертає номер відкритої сцени у переліку getScenePaths, або -1
	int getActiveSceneIndex() const;
	// Робить вказаний файл відкритою сценою, нічого не завантажуючи. Потрібно редактору, коли
	// після режиму гри він сам відновлює сцену, яку гра могла перемкнути
	void setActiveScenePath(const std::string& path);

	// Кількість завантажень сцени від запуску; так інші частини помічають, що сцену замінили
	unsigned int getLoadCount() const;

	// Завершує роботу менеджера сцен, виходячи з програми
	void exit();

private:
	// Читає файл сцени і замінює ним поточну; replacePersistent прибирає й об'єкти, що переживають зміну сцени
	bool loadFile(const std::string& path, bool replacePersistent);

	std::vector<std::string> mPaths;
	std::string mActivePath;
	std::string mRequestedPath;
	unsigned int mLoadCount = 0;
	bool mExitRequested = false;

	// Оновлює менеджер сцен, завантажуючи запитану сцену
	void update();

	friend class AppWindow;
};
