#include "Json.h"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <cstdlib>

// Порожнє значення, яке повертається на запит неіснуючого поля
static const JsonValue& nullValue()
{
	static const JsonValue empty;
	return empty;
}

// Перетворює число на найкоротший запис, який читається назад без втрат
static std::string numberToString(double value)
{
	// JSON не має запису для нескінченності та NaN, тому такі числа зберігаємо як нуль
	if (!std::isfinite(value)) return "0";

	// Цілі числа пишемо без дробової частини, щоб файл лишався охайним
	if (value == (double)(long long)value && std::fabs(value) < 1e15)
	{
		return std::to_string((long long)value);
	}

	// Числа рушія — це float, тому й точність підбираємо за поверненням саме у float:
	// інакше кожен кут займав би сімнадцять цифр без жодної користі
	bool isFloat = (double)(float)value == value;

	for (int precision = 6; precision <= 17; precision++)
	{
		std::ostringstream out;
		out << std::setprecision(precision) << value;

		std::string text = out.str();

		if (isFloat)
		{
			if (std::strtof(text.c_str(), nullptr) == (float)value) return text;
		}
		else if (std::strtod(text.c_str(), nullptr) == value)
		{
			return text;
		}
	}

	std::ostringstream out;
	out << std::setprecision(17) << value;

	return out.str();
}

// Дописує рядок у лапках, екрануючи символи, які JSON не дозволяє лишати як є
static void writeString(std::string& out, const std::string& text)
{
	out += '"';

	for (unsigned char symbol : text)
	{
		switch (symbol)
		{
		case '"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (symbol < 0x20)
			{
				// Керівні символи записуються числом, бо у тексті JSON вони заборонені
				char buffer[8];
				snprintf(buffer, sizeof(buffer), "\\u%04x", symbol);
				out += buffer;
			}
			else
			{
				out += (char)symbol;
			}
			break;
		}
	}

	out += '"';
}

JsonValue::JsonValue() : mType(Type::Null) {}
JsonValue::JsonValue(bool value) : mType(Type::Bool), mBool(value) {}
JsonValue::JsonValue(int value) : mType(Type::Number), mNumber((double)value) {}
JsonValue::JsonValue(float value) : mType(Type::Number), mNumber((double)value) {}
JsonValue::JsonValue(double value) : mType(Type::Number), mNumber(value) {}
JsonValue::JsonValue(const char* value) : mType(Type::String), mString(value ? value : "") {}
JsonValue::JsonValue(const std::string& value) : mType(Type::String), mString(value) {}

// Створює порожній об'єкт
JsonValue JsonValue::object()
{
	JsonValue value;
	value.mType = Type::Object;
	return value;
}

// Створює порожній масив
JsonValue JsonValue::array()
{
	JsonValue value;
	value.mType = Type::Array;
	return value;
}

// Повертає тип значення
JsonValue::Type JsonValue::getType() const
{
	return mType;
}

// Перевіряє, чи є у об'єкта поле з таким іменем
bool JsonValue::has(const char* key) const
{
	if (mType != Type::Object) return false;

	for (const auto& field : mObject)
	{
		if (field.first == key) return true;
	}

	return false;
}

// Повертає поле об'єкта, або порожнечу, якщо його немає
const JsonValue& JsonValue::get(const char* key) const
{
	if (mType == Type::Object)
	{
		for (const auto& field : mObject)
		{
			if (field.first == key) return field.second;
		}
	}

	return nullValue();
}

// Додає або замінює поле об'єкта
void JsonValue::set(const char* key, const JsonValue& value)
{
	if (mType != Type::Object)
	{
		mType = Type::Object;
		mObject.clear();
	}

	for (auto& field : mObject)
	{
		if (field.first == key)
		{
			field.second = value;
			return;
		}
	}

	mObject.push_back(std::make_pair(std::string(key), value));
}

// Повертає кількість елементів масиву
size_t JsonValue::size() const
{
	if (mType == Type::Array) return mArray.size();
	if (mType == Type::Object) return mObject.size();

	return 0;
}

// Повертає елемент масиву за номером, або порожнечу поза межами
const JsonValue& JsonValue::at(size_t index) const
{
	if (mType != Type::Array || index >= mArray.size()) return nullValue();

	return mArray[index];
}

// Додає елемент у кінець масиву
void JsonValue::push(const JsonValue& value)
{
	if (mType != Type::Array)
	{
		mType = Type::Array;
		mArray.clear();
	}

	mArray.push_back(value);
}

// Читає значення потрібного типу, повертаючи запасне, якщо тип не той
float JsonValue::asFloat(float fallback) const
{
	if (mType == Type::Number) return (float)mNumber;

	return fallback;
}

// Читає значення потрібного типу, повертаючи запасне, якщо тип не той
int JsonValue::asInt(int fallback) const
{
	if (mType == Type::Number) return (int)mNumber;

	return fallback;
}

// Читає значення потрібного типу, повертаючи запасне, якщо тип не той
bool JsonValue::asBool(bool fallback) const
{
	if (mType == Type::Bool) return mBool;

	// Старіші файли писали прапорці числами, тому нуль і одиниця теж приймаються
	if (mType == Type::Number) return mNumber != 0.0;

	return fallback;
}

// Читає значення потрібного типу, повертаючи запасне, якщо тип не той
std::string JsonValue::asString(const std::string& fallback) const
{
	if (mType == Type::String) return mString;

	return fallback;
}

// Дописує власний текст у вихідний рядок із заданою глибиною вкладеності
void JsonValue::write(std::string& out, int depth) const
{
	std::string indent((size_t)depth * 2 + 2, ' ');
	std::string closingIndent((size_t)depth * 2, ' ');

	switch (mType)
	{
	case Type::Null: out += "null"; break;
	case Type::Bool: out += mBool ? "true" : "false"; break;
	case Type::Number: out += numberToString(mNumber); break;
	case Type::String: writeString(out, mString); break;

	case Type::Array:
	{
		if (mArray.empty()) { out += "[]"; break; }

		// Масив самих чисел лишається в один рядок: так вектори та кольори читаються краще
		bool compact = true;

		for (const JsonValue& item : mArray)
		{
			if (item.mType != Type::Number && item.mType != Type::Bool) { compact = false; break; }
		}

		out += '[';

		for (size_t i = 0; i < mArray.size(); i++)
		{
			if (i > 0) out += ',';

			if (compact) { if (i > 0) out += ' '; }
			else { out += '\n'; out += indent; }

			mArray[i].write(out, depth + 1);
		}

		if (!compact) { out += '\n'; out += closingIndent; }

		out += ']';
		break;
	}

	case Type::Object:
	{
		if (mObject.empty()) { out += "{}"; break; }

		out += '{';

		for (size_t i = 0; i < mObject.size(); i++)
		{
			if (i > 0) out += ',';

			out += '\n';
			out += indent;

			writeString(out, mObject[i].first);
			out += ": ";

			mObject[i].second.write(out, depth + 1);
		}

		out += '\n';
		out += closingIndent;
		out += '}';
		break;
	}
	}
}

// Перетворює значення на текст з відступами
std::string JsonValue::toString() const
{
	std::string out;

	write(out, 0);
	out += '\n';

	return out;
}

// Розбирач тексту JSON, що йде по рядку зліва направо
namespace
{
	struct JsonParser
	{
		const std::string& text;
		size_t pos = 0;
		std::string error;

		JsonParser(const std::string& source) : text(source) {}

		// Пропускає пробіли та переводи рядка між значеннями
		void skipSpace()
		{
			while (pos < text.size())
			{
				char symbol = text[pos];

				if (symbol == ' ' || symbol == '\t' || symbol == '\n' || symbol == '\r') pos++;
				else break;
			}
		}

		// Запам'ятовує першу помилку разом з місцем, де вона трапилась
		bool fail(const std::string& message)
		{
			if (error.empty())
			{
				error = message + " at offset " + std::to_string(pos);
			}

			return false;
		}

		// Перевіряє наступний символ і з'їдає його, якщо він очікуваний
		bool expect(char symbol)
		{
			skipSpace();

			if (pos < text.size() && text[pos] == symbol) { pos++; return true; }

			return fail(std::string("expected '") + symbol + "'");
		}

		// Читає рядок у лапках разом з екранованими символами
		bool parseString(std::string& out)
		{
			if (!expect('"')) return false;

			out.clear();

			while (pos < text.size())
			{
				char symbol = text[pos++];

				if (symbol == '"') return true;

				if (symbol != '\\') { out += symbol; continue; }

				if (pos >= text.size()) return fail("unfinished escape");

				char escaped = text[pos++];

				switch (escaped)
				{
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case '/': out += '/'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case 'n': out += '\n'; break;
				case 'r': out += '\r'; break;
				case 't': out += '\t'; break;
				case 'u':
				{
					if (pos + 4 > text.size()) return fail("short \\u escape");

					int code = (int)strtol(text.substr(pos, 4).c_str(), nullptr, 16);
					pos += 4;

					// Шляхи та імена рушія лишаються у межах ASCII, тому ширші символи опускаємо
					if (code < 0x80) out += (char)code;

					break;
				}
				default: return fail("unknown escape");
				}
			}

			return fail("unfinished string");
		}

		// Читає будь-яке значення JSON
		bool parseValue(JsonValue& out)
		{
			skipSpace();

			if (pos >= text.size()) return fail("unexpected end");

			char symbol = text[pos];

			if (symbol == '{')
			{
				pos++;
				out = JsonValue::object();

				skipSpace();

				if (pos < text.size() && text[pos] == '}') { pos++; return true; }

				while (true)
				{
					std::string key;

					if (!parseString(key)) return false;
					if (!expect(':')) return false;

					JsonValue value;

					if (!parseValue(value)) return false;

					out.set(key.c_str(), value);

					skipSpace();

					if (pos < text.size() && text[pos] == ',') { pos++; continue; }

					return expect('}');
				}
			}

			if (symbol == '[')
			{
				pos++;
				out = JsonValue::array();

				skipSpace();

				if (pos < text.size() && text[pos] == ']') { pos++; return true; }

				while (true)
				{
					JsonValue value;

					if (!parseValue(value)) return false;

					out.push(value);

					skipSpace();

					if (pos < text.size() && text[pos] == ',') { pos++; continue; }

					return expect(']');
				}
			}

			if (symbol == '"')
			{
				std::string value;

				if (!parseString(value)) return false;

				out = JsonValue(value);
				return true;
			}

			if (text.compare(pos, 4, "true") == 0) { pos += 4; out = JsonValue(true); return true; }
			if (text.compare(pos, 5, "false") == 0) { pos += 5; out = JsonValue(false); return true; }
			if (text.compare(pos, 4, "null") == 0) { pos += 4; out = JsonValue(); return true; }

			// Лишається число: його межі визначає сам strtod
			const char* start = text.c_str() + pos;
			char* end = nullptr;

			double number = std::strtod(start, &end);

			if (end == start) return fail("expected value");

			pos += (size_t)(end - start);

			out = JsonValue(number);
			return true;
		}
	};
}

// Розбирає текст JSON; за помилки повертає порожнечу та пояснення через error
JsonValue JsonValue::parse(const std::string& text, std::string* error)
{
	JsonParser parser(text);

	JsonValue result;

	if (!parser.parseValue(result))
	{
		if (error) *error = parser.error;

		return JsonValue();
	}

	parser.skipSpace();

	// Зайвий текст після значення означає, що файл не такий, як ми думали
	if (parser.pos != text.size())
	{
		if (error) *error = "trailing text at offset " + std::to_string(parser.pos);

		return JsonValue();
	}

	if (error) error->clear();

	return result;
}
