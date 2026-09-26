#pragma once
#include "Json.h"
#include <string>
#include <vector>
#include <map>
#include <set>

class Entity;
class Prefab;
class Component;

// Префаби у вигляді файлів: створення з об'єктів сцени, їхні екземпляри, зміни екземплярів
// відносно префаба та поширення правок префаба на всі екземпляри
class PrefabLibrary
{
public:
	// Повертає єдиний екземпляр бібліотеки (синглтон)
	static PrefabLibrary* get();

	// Тека, у якій лежать файли префабів
	static const char* FOLDER;

	// Перечитує перелік файлів префабів у теці
	void refresh();
	// Повертає шляхи всіх знайдених префабів
	const std::vector<std::string>& getPaths() const;
	// Повертає ім'я префаба без теки й розширення
	static std::string getName(const std::string& path);

	// Повертає вміст префаба з файлу, або nullptr, якщо файлу немає чи він зіпсований
	const JsonValue* getData(const std::string& path);

	// Повертає прихований образ префаба, на який посилаються поля компонентів гри
	Prefab* getTemplate(const std::string& path);
	// Повертає шлях префаба, якщо об'єкт — його образ, інакше порожній рядок
	std::string getTemplatePath(Entity* entity) const;

	// Зберігає об'єкт разом з нащадками як новий префаб і робить сам об'єкт його екземпляром.
	// Повертає шлях до файлу або порожній рядок, якщо записати не вдалося
	std::string createAsset(Entity* root);
	// Створює у сцені новий екземпляр префаба; parent може бути nullptr
	Entity* instantiate(const std::string& path, Entity* parent);

	// Повертає корінь екземпляра, до якого належить об'єкт, або nullptr
	static Entity* findInstanceRoot(Entity* entity);

	// Повертає ключі властивостей, якими екземпляр відрізняється від свого префаба
	std::set<std::string> computeOverrides(Entity* instanceRoot);
	// Приводить об'єкт до опису префаба на місці, не чіпаючи перелічених змінених властивостей,
	// а також позиції й повороту кореня. Об'єкти й компоненти, що вже збігаються, лишаються тими
	// самими, тож посилання на них з інших частин сцени не рвуться
	void sync(Entity* root, const JsonValue& data, const std::set<std::string>& overrides, bool isTemplate);

	// Записує стан екземпляра у префаб і поширює зміну на інші екземпляри та образ
	void apply(Entity* instanceRoot);
	// Повертає екземпляру стан префаба, крім позиції й повороту кореня
	void revert(Entity* instanceRoot);
	// Розриває зв'язок екземпляра з префабом, лишаючи сам об'єкт як є
	void unpack(Entity* instanceRoot);

	// Записує вміст префаба з об'єкта в режимі редагування префаба. Екземпляри сцени наздоганяють
	// файл, коли сцену відновлюють, а прихований образ оновлюється одразу
	bool saveAsset(const std::string& path, Entity* root);

	// Ключ властивості об'єкта; path — шлях об'єкта всередині префаба
	static std::string entityKey(const std::string& path, const char* field);
	// Ключ поля компонента, що визначається типом і порядковим номером серед однотипних
	static std::string componentKey(const std::string& path, const std::string& type, int occurrence, const std::string& field);
	// Шлях об'єкта всередині екземпляра: номери дочірніх від кореня, як-от "/0/2"
	static std::string pathInInstance(Entity* entity, Entity* instanceRoot);
	// Порядковий номер компонента серед компонентів того самого типу в його об'єкті
	static int componentOccurrence(Component* component);

private:
	PrefabLibrary();

	// Записує вміст префаба у файл
	bool writeFile(const std::string& path, const JsonValue& data);

	std::vector<std::string> mPaths;
	std::map<std::string, JsonValue> mData;
	std::map<std::string, Prefab*> mTemplates;

	// Образи, що саме будуються: префаб, який посилається сам на себе, інакше будувався б вічно
	std::set<std::string> mBuilding;
};
