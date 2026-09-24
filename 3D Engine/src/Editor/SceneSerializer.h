#pragma once
#include <string>

// Зберігає та відновлює сцену у простому текстовому форматі, де кожен рядок
// починається з ключа, а решта рядка є його значенням
class SceneSerializer
{
public:
	// Записує поточну сцену у текст
	static std::string serialize();
	// Відновлює сцену з тексту, знищивши те, що було у сцені до цього
	static void deserialize(const std::string& text);

	// Зберігає сцену у файл
	static bool saveToFile(const std::string& path);
	// Завантажує сцену з файлу
	static bool loadFromFile(const std::string& path);
};
