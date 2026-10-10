#include "AssetCache.h"
#include <fstream>
#include <cwctype>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace filesystem = std::experimental::filesystem;

// Мітка на початку кожного файлу кешу, за якою чужий чи обрізаний файл не приймається
static const char MAGIC[4] = { 'E', 'C', 'H', '1' };

// Повертає шлях файлу кешу для ресурсу: Library\тека\ім'я_хеш.розширення
std::wstring AssetCache::pathOf(const wchar_t* folder, const std::wstring& source, const wchar_t* extension)
{
	// Хеш повного шляху розрізняє однакові імена в різних теках, а перенесений проєкт отримує новий кеш
	std::wstring full = source;
	size_t separator = full.find(L"::");

	full = filesystem::absolute(full.substr(0, separator)).wstring() + (separator == std::wstring::npos ? std::wstring() : full.substr(separator));

	unsigned long long hash = 1469598103934665603ull;

	for (wchar_t c : full)
	{
		hash ^= (unsigned long long)std::towlower(c);
		hash *= 1099511628211ull;
	}

	// Ім'я файлу лишається впізнаваним, а двокрапки вузла моделі й інші недозволені символи замінюються
	std::wstring name = filesystem::path(full.substr(0, separator)).filename().wstring();

	if (separator != std::wstring::npos) name += L"_" + full.substr(separator + 2);

	for (wchar_t& c : name)
	{
		if (c == L':' || c == L'\\' || c == L'/' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') c = L'_';
	}

	if (name.size() > 60) name.resize(60);

	wchar_t hex[17];
	swprintf_s(hex, L"%016llx", hash);

	return std::wstring(L"Library\\") + folder + L"\\" + name + L"_" + hex + extension;
}

// Повертає відбиток файлу з часу зміни й розміру, або порожній рядок, якщо файлу немає
std::string AssetCache::stampOf(const std::wstring& file)
{
	std::error_code error;

	if (!filesystem::is_regular_file(file, error)) return std::string();

	long long time = (long long)filesystem::last_write_time(file, error).time_since_epoch().count();
	unsigned long long size = (unsigned long long)filesystem::file_size(file, error);

	return std::to_string(time) + ":" + std::to_string(size);
}

// Читає дані кешу, якщо файл є і його відбиток збігається з очікуваним
bool AssetCache::read(const std::wstring& path, const std::string& stamp, std::vector<unsigned char>& data)
{
	if (stamp.empty()) return false;

	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file.is_open()) return false;

	size_t total = (size_t)file.tellg();
	file.seekg(0);

	char magic[4] = {};
	unsigned int stampSize = 0;

	file.read(magic, 4);
	file.read((char*)&stampSize, sizeof(stampSize));

	if (!file || memcmp(magic, MAGIC, 4) != 0 || stampSize != stamp.size()) return false;

	std::string stored(stampSize, '\0');
	file.read(&stored[0], stampSize);

	if (!file || stored != stamp) return false;

	size_t header = 4 + sizeof(stampSize) + stampSize;

	data.resize(total - header);
	file.read((char*)data.data(), (std::streamsize)data.size());

	return (bool)file;
}

// Записує дані кешу з відбитком через тимчасовий файл, щоб обірваний запис не лишив зіпсований кеш
bool AssetCache::write(const std::wstring& path, const std::string& stamp, const void* data, size_t size)
{
	if (stamp.empty()) return false;

	std::error_code error;
	filesystem::create_directories(filesystem::path(path).parent_path(), error);

	std::wstring temporary = path + L".tmp";

	{
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);

		if (!file.is_open()) return false;

		unsigned int stampSize = (unsigned int)stamp.size();

		file.write(MAGIC, 4);
		file.write((const char*)&stampSize, sizeof(stampSize));
		file.write(stamp.data(), stampSize);
		file.write((const char*)data, (std::streamsize)size);

		if (!file) return false;
	}

	filesystem::remove(path, error);
	filesystem::rename(temporary, path, error);

	return !error;
}
