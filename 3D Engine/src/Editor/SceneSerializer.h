#pragma once
#include "Json.h"
#include <string>
#include <vector>

class Entity;
class Component;
class Renderer;
class Material;

// Зберігає та відновлює сцену у форматі JSON
class SceneSerializer
{
public:
	// Записує поточну сцену у текст. Об'єкти, що переживають зміну сцени, у файл не потрапляють,
	// бо завантаження створило б їх копію; знімок для режиму гри просить їх записати теж
	static std::string serialize(bool includePersistent = false);
	// Відновлює сцену з тексту, знищивши те, що було у сцені до цього. Повертає false, якщо текст
	// не вдалося прочитати, і тоді сцену не чіпає. Створені об'єкти віддає у порядку файлу.
	// replacePersistent знищує й такі об'єкти, щоб на їхнє місце стали записані у знімку
	static bool deserialize(const std::string& text, std::vector<Entity*>* created = nullptr, bool replacePersistent = false);

	// Зберігає сцену у файл
	static bool saveToFile(const std::string& path);
	// Завантажує сцену з файлу
	static bool loadFromFile(const std::string& path);

	// Далі — будівельні блоки збереження сцени, якими користуються й префаби

	// Записує об'єкт разом з усіма нащадками у тому самому форматі, що й сцену. Позиція й поворот
	// кореня обнуляються, бо кожен екземпляр префаба має власні, а посилання на об'єкти поза
	// піддеревом губляться, крім посилань на префаби-файли
	static JsonValue serializeSubtree(Entity* root);
	// Створює об'єкти з опису, не чіпаючи решту сцени; кореневі стають дочірніми для parent.
	// asTemplate будує прихований образ префаба: його компоненти не прокидаються, матеріали
	// переживають зміну сцени, а самі об'єкти не показуються в дереві сцени
	static std::vector<Entity*> buildSubtree(const JsonValue& data, Entity* parent, bool asTemplate);

	// Складає опис одного матеріалу
	static JsonValue writeMaterial(Material* material);
	// Переносить у матеріал поля з опису, замінюючи його текстури
	static void readMaterial(Material* material, const JsonValue& data);

	// Створює компонент за описом; самі поля потім задають applyRenderer та applyProperties
	static Component* createComponent(Entity* entity, const JsonValue& data);
	// Переносить у рендер-компонент меш і тіні з опису, якщо вони в ньому є
	static void applyRenderer(Renderer* renderer, const JsonValue& data);
	// Переносить у компонент його власні поля з опису; посилання на об'єкти шукаються в entities
	static void applyProperties(Component* component, const JsonValue& data, const std::vector<Entity*>& entities);
};
