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

namespace
{
	// A 32-bit Windows program's resources: find (type, name or id) and return its bytes
	struct ExeResources
	{
		std::vector<uint8_t> d;
		size_t base = 0;
		size_t sections = 0;
		int sectionCount = 0;

		bool open(const std::string& exePath)
		{
			std::ifstream in(exePath, std::ios::binary);
			if (!in)
				return false;
			d.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z')
				return false;
			size_t pe = U32(d, 0x3c);
			if (pe + 0xF8 > d.size() || U32(d, pe) != 0x00004550 || U16(d, pe + 24) != 0x10B)
				return false; // only 32-bit PE programs
			size_t optional = pe + 24;
			sections = optional + U16(d, pe + 20);
			sectionCount = U16(d, pe + 6);
			base = to_file(U32(d, optional + 96 + 8 * 2));
			return base != 0;
		}

		size_t to_file(uint32_t rva) const
		{
			for (int i = 0; i < sectionCount; i++)
			{
				size_t s = sections + 40 * i;
				auto va = U32(d, s + 12), virtualSize = U32(d, s + 8), raw = U32(d, s + 20), rawSize = U32(d, s + 16);
				if (rva >= va && rva < va + std::max(virtualSize, rawSize) && rva - va < rawSize)
					return raw + (rva - va);
			}
			return 0;
		}

		// Entry in a resource directory by id (id < 0: the first one) or by name; returns the raw target field
		uint32_t find(size_t dir, int id, const char* name) const
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
		}

		// The bytes of resource (type, id or name), first language
		bool get(int type, int id, const char* name, size_t& data, uint32_t& size) const
		{
			auto t = find(0, type, nullptr);
			if (!(t & 0x80000000))
				return false;
			auto n = find(t & 0x7FFFFFFF, id, name);
			if (!(n & 0x80000000))
				return false;
			auto language = find(n & 0x7FFFFFFF, -1, nullptr);
			if (!language || language & 0x80000000)
				return false;
			size_t entry = base + language;
			data = to_file(U32(d, entry));
			size = U32(d, entry + 4);
			return data && data + size <= d.size();
		}
	};
}

SDL_Surface* table_art::LoadExeBitmap(const std::string& exePath, const char* resourceName)
{
	ExeResources exe;
	size_t data;
	uint32_t dataSize;
	if (!exe.open(exePath) || !exe.get(2, 0, resourceName, data, dataSize) || dataSize < 40)
		return nullptr;
	auto& d = exe.d;
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

std::string table_art::Text(int id, const std::string& fallback)
{
	static std::map<int, std::string> texts;
	static std::string loadedFor = "?";
	if (loadedFor != pb::DatFileName)
	{
		loadedFor = pb::DatFileName;
		texts.clear();
		ExeResources exe;
		auto path = table_exe();
		if (!path.empty() && exe.open(path))
		{
			// RT_STRING (6): block n holds the strings (n-1)*16 .. (n-1)*16+15, each a UTF-16 length + text
			auto type = exe.find(0, 6, nullptr);
			if (type & 0x80000000)
			{
				size_t dir = type & 0x7FFFFFFF;
				auto count = U16(exe.d, exe.base + dir + 12) + U16(exe.d, exe.base + dir + 14);
				for (int i = 0; i < count; i++)
				{
					auto blockId = static_cast<int>(U32(exe.d, exe.base + dir + 16 + 8 * i));
					size_t data;
					uint32_t size;
					if (blockId & 0x80000000 || !exe.get(6, blockId, nullptr, data, size))
						continue;
					size_t p = data;
					for (int s = 0; s < 16 && p + 2 <= data + size; s++)
					{
						auto length = U16(exe.d, p);
						p += 2;
						std::string text;
						for (int c = 0; c < length; c++)
						{
							auto ch = U16(exe.d, p + 2 * c);
							text += ch < 128 ? static_cast<char>(ch) : '?';
						}
						p += 2u * length;
						if (length)
							texts[(blockId - 1) * 16 + s] = text;
					}
				}
			}
		}
	}
	auto found = texts.find(id);
	return found != texts.end() ? found->second : fallback;
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
