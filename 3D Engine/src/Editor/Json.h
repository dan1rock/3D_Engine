#pragma once
#include <string>
#include <vector>
#include <utility>

// Значення JSON: порожнеча, логічне значення, число, рядок, масив або об'єкт.
// Поля об'єкта зберігаються у порядку додавання, щоб файл сцени читався так, як його записали
class JsonValue
{
public:
	enum class Type { Null, Bool, Number, String, Array, Object };

	JsonValue();
	JsonValue(bool value);
	JsonValue(int value);
	JsonValue(float value);
	JsonValue(double value);
	JsonValue(const char* value);
	JsonValue(const std::string& value);

	// Створює порожній об'єкт
	static JsonValue object();
	// Створює порожній масив
	static JsonValue array();

	// Розбирає текст JSON; за помилки повертає порожнечу та пояснення через error
	static JsonValue parse(const std::string& text, std::string* error = nullptr);

	// Перетворює значення на текст з відступами
	std::string toString() const;

	// Повертає тип значення
	Type getType() const;

	// Перевіряє, чи є у об'єкта поле з таким іменем
	bool has(const char* key) const;
	// Повертає поле об'єкта, або порожнечу, якщо його немає
	const JsonValue& get(const char* key) const;
	// Додає або замінює поле об'єкта
	void set(const char* key, const JsonValue& value);
	// Повертає ім'я та значення поля об'єкта за його номером, щоб поля можна було обійти
	const std::string& keyAt(size_t index) const;
	const JsonValue& valueAt(size_t index) const;

	// Повертає кількість елементів масиву
	size_t size() const;
	// Повертає елемент масиву за номером, або порожнечу поза межами
	const JsonValue& at(size_t index) const;
	// Додає елемент у кінець масиву
	void push(const JsonValue& value);

	// Читає значення потрібного типу, повертаючи запасне, якщо тип не той
	float asFloat(float fallback = 0.0f) const;
	int asInt(int fallback = 0) const;
	bool asBool(bool fallback = false) const;
	std::string asString(const std::string& fallback = std::string()) const;

private:
	// Дописує власний текст у вихідний рядок із заданою глибиною вкладеності
	void write(std::string& out, int depth) const;

	Type mType = Type::Null;

	bool mBool = false;
	double mNumber = 0.0;
	std::string mString;

	std::vector<JsonValue> mArray;
	std::vector<std::pair<std::string, JsonValue>> mObject;
};
