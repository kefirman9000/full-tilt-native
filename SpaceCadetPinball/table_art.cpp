#include "pch.h"
#include "table_art.h"

#include <fstream>
#include <iterator>
#include "pb.h"

namespace
{
	uint32_t U32(const std::vector<uint8_t>& d, size_t p)
	{
		return p + 4 <= d.size() ? d[p] | d[p + 1] << 8 | d[p + 2] << 16 | static_cast<uint32_t>(d[p + 3]) << 24 : 0;
	}

	uint16_t U16(const std::vector<uint8_t>& d, size_t p)
	{
		return p + 2 <= d.size() ? static_cast<uint16_t>(d[p] | d[p + 1] << 8) : 0;
	}

	// The table program next to the table data: HERO.DAT -> HERO.EXE / Hero.exe / hero.exe (Linux and macOS care)
	std::string table_exe()
	{
		std::string stem = pb::DatFileName;
		auto slash = stem.find_last_of("/\\");
		if (slash != std::string::npos)
			stem = stem.substr(slash + 1);
		auto dot = stem.find_last_of('.');
		if (dot != std::string::npos)
			stem = stem.substr(0, dot);
		std::string upper = stem, lower = stem, title = stem;
		for (auto& c : upper) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
		for (auto& c : lower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
		title = lower;
		if (!title.empty())
			title[0] = static_cast<char>(toupper(static_cast<unsigned char>(title[0])));
		for (auto& name : {upper + ".EXE", title + ".exe", lower + ".exe", stem + ".exe", stem + ".EXE"})
		{
			auto path = pb::make_path_name(name);
			if (std::ifstream(path, std::ios::binary))
				return path;
		}
		return "";
	}
}

SDL_Surface* table_art::LoadExeBitmap(const std::string& exePath, const char* resourceName)
{
	std::ifstream in(exePath, std::ios::binary);
	if (!in)
		return nullptr;
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z')
		return nullptr;
	size_t pe = U32(d, 0x3c);
	if (pe + 0xF8 > d.size() || U32(d, pe) != 0x00004550 || U16(d, pe + 24) != 0x10B)
		return nullptr; // only 32-bit PE programs
	size_t optional = pe + 24, sections = optional + U16(d, pe + 20);
	auto sectionCount = U16(d, pe + 6);
	auto rsrcRva = U32(d, optional + 96 + 8 * 2);
	auto toFile = [&](uint32_t rva) -> size_t
	{
		for (int i = 0; i < sectionCount; i++)
		{
			size_t s = sections + 40 * i;
			auto va = U32(d, s + 12), virtualSize = U32(d, s + 8), raw = U32(d, s + 20), rawSize = U32(d, s + 16);
			if (rva >= va && rva < va + std::max(virtualSize, rawSize) && rva - va < rawSize)
				return raw + (rva - va);
		}
		return 0;
	};
	size_t base = toFile(rsrcRva);
	if (!base)
		return nullptr;

	// Resource directory: type RT_BITMAP (2) -> entry named resourceName -> first language -> data
	auto find = [&](size_t dir, int id, const char* name) -> uint32_t
	{
		auto count = U16(d, base + dir + 12) + U16(d, base + dir + 14);
		for (int i = 0; i < count; i++)
		{
			auto entryName = U32(d, base + dir + 16 + 8 * i), target = U32(d, base + dir + 16 + 8 * i + 4);
			bool match;
			if (name)
			{
				if (!(entryName & 0x80000000))
					continue;
				size_t s = base + (entryName & 0x7FFFFFFF);
				auto length = U16(d, s);
				std::string text;
				for (int c = 0; c < length; c++)
					text += static_cast<char>(U16(d, s + 2 + 2 * c));
				match = text == name;
			}
			else
				match = id < 0 || (!(entryName & 0x80000000) && static_cast<int>(entryName) == id);
			if (match)
				return target;
		}
		return 0;
	};
	auto type = find(0, 2, nullptr);
	if (!(type & 0x80000000))
		return nullptr;
	auto named = find(type & 0x7FFFFFFF, 0, resourceName);
	if (!(named & 0x80000000))
		return nullptr;
	auto language = find(named & 0x7FFFFFFF, -1, nullptr);
	if (!language || language & 0x80000000)
		return nullptr;
	size_t entry = base + language;
	auto dataRva = U32(d, entry), dataSize = U32(d, entry + 4);
	size_t data = toFile(dataRva);
	if (!data || data + dataSize > d.size() || dataSize < 40)
		return nullptr;

	// A bitmap resource is a BMP file without its 14-byte file header: put one back and let SDL read it
	uint32_t headerSize = U32(d, data), bits = U16(d, data + 14), colors = U32(d, data + 32);
	if (!colors && bits <= 8)
		colors = 1u << bits;
	std::vector<uint8_t> file(14 + dataSize);
	uint32_t offBits = 14 + headerSize + 4 * colors, fileSize = static_cast<uint32_t>(file.size());
	file[0] = 'B';
	file[1] = 'M';
	memcpy(&file[2], &fileSize, 4);
	memcpy(&file[10], &offBits, 4);
	memcpy(&file[14], &d[data], dataSize);
	return SDL_LoadBMP_RW(SDL_RWFromConstMem(file.data(), static_cast<int>(file.size())), 1);
}

SDL_Surface* table_art::Load()
{
	if (auto surface = SDL_LoadBMP(pb::make_path_name("panel.bmp").c_str()))
		return surface;
	auto exe = table_exe();
	if (!exe.empty())
		if (auto surface = LoadExeBitmap(exe, "SPLASH_BITMAP"))
			return surface;
	return SDL_LoadBMP(pb::make_path_name("preview.bmp").c_str());
}
