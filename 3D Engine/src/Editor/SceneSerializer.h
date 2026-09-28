#pragma once
#include "Json.h"
#include <string>
#include <vector>
#include <unordered_map>

class Entity;
class Component;
class Renderer;
class Material;

// Незмінні номери матеріалів для історії змін: матеріал без файлу живе, поки не завантажили іншу сцену
struct MaterialRegistry
{
	std::vector<Material*> materials;
	std::unordered_map<Material*, int> ids;

	// Повертає номер матеріалу, за потреби видаючи новий
	int idOf(Material* material);
};

// Стан сцени для історії змін: запис кожного об'єкта за його номером, значення матеріалів і порядок дерева
struct SceneRecords
{
	std::unordered_map<int, std::string> entities;
	std::unordered_map<int, std::string> materials;
	std::vector<int> order;
};

// Зміна запису одного об'єкта в історії: з якого стану й до якого; порожній запис означає, що об'єкта немає
struct RecordChange
{
	int id = 0;
	std::string from;
	std::string to;
};

// Зберігає та відновлює сцену у форматі JSON
class SceneSerializer
{
public:
	// Записує поточну сцену у текст. Без includePersistent об'єкти, що переживають зміну сцени,
	// пропускаються; файл сцени та знімок режиму гри записують їх теж
	static std::string serialize(bool includePersistent = false);
	// Відновлює сцену з тексту, знищивши те, що було у сцені до цього. Повертає false, якщо текст
	// не вдалося прочитати, і тоді сцену не чіпає. Створені об'єкти віддає у порядку файлу.
	// replacePersistent знищує й такі об'єкти, щоб на їхнє місце стали записані у знімку
	static bool deserialize(const std::string& text, std::vector<Entity*>* created = nullptr, bool replacePersistent = false);

	// Зберігає сцену у файл разом з об'єктами, що переживають зміну сцени: інакше сцена, яка їх
	// створює, після збереження втратила б їх. Завантажує файли сцен SceneManager
	static bool saveToFile(const std::string& path);

	// Далі — будівельні блоки збереження сцени, якими користуються й префаби

	// Записує об'єкт разом з усіма нащадками у тому самому форматі, що й сцену. Позиція й поворот
	// кореня обнуляються, бо кожен екземпляр префаба має власні, а посилання на об'єкти поза
	// піддеревом губляться, крім посилань на префаби-файли
	static JsonValue serializeSubtree(Entity* root);
	// Записує вказані об'єкти з нащадками так само, як сцену: корені у світових координатах і зі зв'язками з префабами
	static std::string serializeEntities(const std::vector<Entity*>& roots);
	// Створює об'єкти з опису, не чіпаючи решту сцени; кореневі стають дочірніми для parent.
	// asTemplate будує прихований образ префаба: його компоненти не прокидаються, матеріали
	// переживають зміну сцени, а самі об'єкти не показуються в дереві сцени
	static std::vector<Entity*> buildSubtree(const JsonValue& data, Entity* parent, bool asTemplate);

	// Записує стан сцени для історії змін: посилання й батьки як номери об'єктів, матеріали як номери реєстру
	static void captureRecords(SceneRecords& out, MaterialRegistry& registry);
	// Переводить названі об'єкти й матеріали між записами, торкаючись лише того, що в записі змінилося
	static void applyRecords(const std::vector<RecordChange>& entities, const std::vector<std::pair<int, std::string>>& materials, const std::vector<int>& order, MaterialRegistry& registry);

	// Складає опис одного матеріалу
	static JsonValue writeMaterial(Material* material);
	// Переносить у матеріал поля з опису, замінюючи його текстури
	static void readMaterial(Material* material, const JsonValue& data);

	// Ставить рендер-компоненту матеріали слотів з опису; registry потрібен для записів історії змін
	static void applyMaterials(Renderer* renderer, const JsonValue& data, bool asTemplate, MaterialRegistry* registry = nullptr);

	// Створює компонент за описом; самі поля потім задають applyRenderer та applyProperties
	static Component* createComponent(Entity* entity, const JsonValue& data);
	// Переносить у рендер-компонент меш і тіні з опису, якщо вони в ньому є
	static void applyRenderer(Renderer* renderer, const JsonValue& data);
	// Переносить у компонент його власні поля з опису; посилання на об'єкти шукаються в entities
	static void applyProperties(Component* component, const JsonValue& data, const std::vector<Entity*>& entities);
};
