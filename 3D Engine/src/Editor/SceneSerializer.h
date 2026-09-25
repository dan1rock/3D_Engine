#pragma once
#include <string>
#include <vector>

class Entity;

// Зберігає та відновлює сцену у простому текстовому форматі, де кожен рядок
// починається з ключа, а решта рядка є його значенням
class SceneSerializer
{
public:
	// Записує поточну сцену у текст. Об'єкти, що переживають зміну сцени, у файл не потрапляють,
	// бо завантаження створило б їх копію; знімок для режиму гри просить їх записати теж
	static std::string serialize(bool includePersistent = false);
	// Відновлює сцену з тексту, знищивши те, що було у сцені до цього. Повертає false, якщо текст
	// не вдалося прочитати, і тоді сцену не чіпає. Створені об'єкти віддає у порядку файлу
	// replacePersistent знищує й такі об'єкти, щоб на їхнє місце стали записані у знімку
	static bool deserialize(const std::string& text, std::vector<Entity*>* created = nullptr, bool replacePersistent = false);

	// Зберігає сцену у файл
	static bool saveToFile(const std::string& path);
	// Завантажує сцену з файлу
	static bool loadFromFile(const std::string& path);
};
