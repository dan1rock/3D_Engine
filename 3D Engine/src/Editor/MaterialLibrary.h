#pragma once
#include "Json.h"
#include <string>
#include <vector>
#include <map>

class Material;
class Mesh;
class Renderer;

// Матеріали у вигляді файлів .mat: рендер-компоненти посилаються на них шляхом
class MaterialLibrary
{
public:
	// Повертає єдиний екземпляр бібліотеки (синглтон)
	static MaterialLibrary* get();

	// Тека, у якій лежать файли матеріалів
	static const char* FOLDER;
	// Розширення файлів матеріалів
	static const char* EXTENSION;
	// Назва вбудованого матеріалу за замовчуванням, якою на нього посилаються файли сцен
	static const char* DEFAULT_NAME;
	// Номер формату сцен і префабів, з якого матеріали стали посиланнями на файли
	static const int REFERENCE_VERSION = 6;

	// Перечитує перелік файлів матеріалів у теці
	void refresh();
	// Повертає шляхи всіх знайдених матеріалів, упорядковані за ім'ям
	const std::vector<std::string>& getPaths() const;
	// Повертає ім'я матеріалу без теки й розширення
	static std::string getName(const std::string& path);

	// Повертає матеріал з файлу, завантажуючи його лише раз; nullptr, якщо файлу немає чи він зіпсований
	Material* load(const std::string& path);
	// Повертає матеріал за посиланням з файлу сцени; відсутній файл замінюється матеріалом за замовчуванням
	Material* find(const std::string& reference);
	// Повертає посилання на матеріал для файлу сцени: DefaultMaterial, шлях файлу, або порожнє для матеріалу без файлу
	static std::string referenceOf(Material* material);
	// Повертає посилання так, як його записано в тексті файлу JSON: у лапках і з подвоєними скісними рисками
	static std::string quotedReference(const std::string& reference);
	// Перевіряє, чи це вбудований матеріал за замовчуванням, який не редагується
	static bool isDefault(Material* material);
	// Перевіряє, чи матеріал вбудований у файл моделі й тому лише для перегляду
	static bool isEmbedded(Material* material);
	// Повертає шлях моделі, у яку вбудовано матеріал, або порожній рядок
	static std::string embeddedModel(Material* material);

	// Повертає шлях файлу моделі меша відносно теки проєкту, без імені вузла
	static std::string modelPathOf(Mesh* mesh);
	// Повертає імена матеріалів моделі за слотами; порожнє ім'я означає слот, якого модель не описує
	std::vector<std::string> modelMaterialNames(Mesh* mesh);
	// Повертає матеріал слота моделі: заміну з налаштувань імпорту або вбудований; nullptr, якщо модель слота не описує
	Material* modelMaterial(Mesh* mesh, unsigned int slot);
	// Повертає вбудований матеріал моделі за іменем, не зважаючи на заміну
	Material* embeddedMaterial(const std::string& modelPath, const std::string& name);
	// Ставить рендер-компоненту матеріали моделі в усі слоти, як їх описує її файл
	void applyModelMaterials(Renderer* renderer);

	// Повертає файл матеріалу, яким замінено вбудований матеріал моделі, або порожній рядок
	std::string getRemap(const std::string& modelPath, const std::string& name);
	// Замінює вбудований матеріал моделі файлом матеріалу і оновлює об'єкти сцени; порожній шлях прибирає заміну
	void setRemap(const std::string& modelPath, const std::string& name, const std::string& materialPath);
	// Витягує вбудовані матеріали моделі у файли разом з вбудованими текстурами й замінює ними вбудовані; повертає кількість
	int extractMaterials(const std::string& modelPath);
	// Повертає всі вже завантажені матеріали з файлів
	std::vector<Material*> getLoaded() const;

	// Створює файл матеріалу з налаштуваннями source або типовими; повертає шлях або порожній рядок
	std::string createAsset(const std::string& name, Material* source = nullptr);
	// Перейменовує файл матеріалу й виправляє посилання у файлах сцен і префабів; повертає новий шлях або порожній рядок
	std::string rename(Material* material, const std::string& newName);
	// Записує у файли матеріали, змінені від останнього запису
	void saveChanged();

	// Переводить старі сцени й префаби з вбудованими матеріалами на файли матеріалів; повертає, чи опис змінився
	bool upgrade(JsonValue& data);
	// Переводить файл сцени чи префаба на файли матеріалів і перезаписує його, якщо було що переводити
	bool upgradeFile(const std::string& path);

private:
	MaterialLibrary();

	// Повертає вільний шлях файлу для імені, доповнюючи ім'я номером, якщо такий файл уже є
	std::string uniquePath(const std::string& name) const;
	// Записує матеріал у його файл
	bool write(Material* material);
	// Повертає файл матеріалу з такими самими налаштуваннями, як в описі, або створює новий з вказаним ім'ям
	std::string adopt(const JsonValue& materialData, const std::string& name);
	// Повертає заміни вбудованих матеріалів моделі, прочитавши її файл налаштувань імпорту за потреби
	std::map<std::string, std::string>& remapsOf(const std::string& modelPath);

	std::vector<std::string> mPaths;
	std::map<std::string, Material*> mLoaded;
	// Останній записаний у файл вміст кожного матеріалу, з яким порівнюються зміни
	std::map<Material*, std::string> mSaved;
	// Старі шляхи перейменованих матеріалів, щоб давні посилання, як-от в історії змін, досі знаходили матеріал
	std::map<std::string, std::string> mRenamed;
	// Вбудовані матеріали моделей за посиланням модель::ім'я
	std::map<std::string, Material*> mEmbedded;
	// Заміни вбудованих матеріалів кожної моделі: ім'я матеріалу моделі -> файл матеріалу
	std::map<std::string, std::map<std::string, std::string>> mRemaps;
};
