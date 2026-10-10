#pragma once
#include <string>
#include <vector>
#include <cstring>

// Кеш перетворених ресурсів у теці Library: заголовок файлу зберігає відбиток джерела, за яким видно, чи кеш ще дійсний
namespace AssetCache
{
	// Повертає шлях файлу кешу для ресурсу: Library\тека\ім'я_хеш.розширення
	std::wstring pathOf(const wchar_t* folder, const std::wstring& source, const wchar_t* extension);
	// Повертає відбиток файлу з часу зміни й розміру, або порожній рядок, якщо файлу немає
	std::string stampOf(const std::wstring& file);
	// Читає дані кешу, якщо файл є і його відбиток збігається з очікуваним
	bool read(const std::wstring& path, const std::string& stamp, std::vector<unsigned char>& data);
	// Записує дані кешу з відбитком через тимчасовий файл, щоб обірваний запис не лишив зіпсований кеш
	bool write(const std::wstring& path, const std::string& stamp, const void* data, size_t size);
}

// Послідовно дописує значення в буфер кешу
class CacheWriter
{
public:
	// Записані байти
	std::vector<unsigned char> data;

	// Дописує сирі байти
	void bytes(const void* source, size_t size)
	{
		const unsigned char* begin = (const unsigned char*)source;
		data.insert(data.end(), begin, begin + size);
	}

	// Дописує значення простого типу як є
	template<class T> void value(const T& v) { bytes(&v, sizeof(T)); }

	// Дописує рядок з його довжиною
	void string(const std::string& s) { value((unsigned int)s.size()); bytes(s.data(), s.size()); }

	// Дописує широкий рядок з його довжиною
	void wstring(const std::wstring& s) { value((unsigned int)s.size()); bytes(s.data(), s.size() * sizeof(wchar_t)); }

	// Дописує масив значень простого типу з його довжиною
	template<class T> void array(const std::vector<T>& v) { value((unsigned int)v.size()); bytes(v.data(), v.size() * sizeof(T)); }
};

// Послідовно читає значення з буфера кешу; вихід за межі позначає все читання невдалим
class CacheReader
{
public:
	// Починає читання з початку буфера
	CacheReader(const std::vector<unsigned char>& source) : mData(source) {}

	// Чи всі значення прочитано в межах буфера
	bool ok = true;

	// Читає сирі байти
	void bytes(void* target, size_t size)
	{
		if (!ok || mOffset + size > mData.size()) { ok = false; return; }

		if (size > 0) memcpy(target, mData.data() + mOffset, size);
		mOffset += size;
	}

	// Читає значення простого типу
	template<class T> T value() { T v{}; bytes(&v, sizeof(T)); return v; }

	// Читає довжину, перевіряючи, що стільки елементів справді вміщається в решту буфера
	size_t count(size_t elementSize)
	{
		unsigned int n = value<unsigned int>();

		if (!ok || (elementSize > 0 && (size_t)n > (mData.size() - mOffset) / elementSize)) { ok = false; return 0; }

		return n;
	}

	// Читає рядок з його довжиною
	std::string string() { std::string s(count(1), '\0'); bytes(&s[0], s.size()); return s; }

	// Читає широкий рядок з його довжиною
	std::wstring wstring() { std::wstring s(count(sizeof(wchar_t)), L'\0'); bytes(&s[0], s.size() * sizeof(wchar_t)); return s; }

	// Читає масив значень простого типу з його довжиною
	template<class T> void array(std::vector<T>& v) { v.resize(count(sizeof(T))); bytes(v.data(), v.size() * sizeof(T)); }

private:
	const std::vector<unsigned char>& mData;
	size_t mOffset = 0;
};
