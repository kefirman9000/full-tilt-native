#include "ft1_strings.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>

namespace
{
	std::map<int, std::string> Strings;
	bool IsLoaded = false;

	uint16_t U16(const std::vector<uint8_t>& d, size_t p)
	{
		return p + 2 <= d.size() ? static_cast<uint16_t>(d[p] | (d[p + 1] << 8)) : 0;
	}
}

bool ft1_strings::Load(const std::string& exePath)
{
	Strings.clear();
	IsLoaded = false;
	std::ifstream in(exePath, std::ios::binary);
	if (!in)
		return false;
	std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (d.size() < 0x40 || d[0] != 'M' || d[1] != 'Z')
		return false;
	size_t ne = U16(d, 0x3c) | (static_cast<size_t>(U16(d, 0x3e)) << 16);
	if (ne + 0x40 > d.size() || d[ne] != 'N' || d[ne + 1] != 'E')
		return false;

	// NE resource table: alignment shift, then (type id, count, reserved, entries[count]) until type id 0.
	// RT_STRING (0x8006) resources hold 16 counted strings each; resource n has ids (n-1)*16 .. (n-1)*16+15.
	size_t rt = ne + U16(d, ne + 0x24);
	unsigned shift = U16(d, rt);
	size_t p = rt + 2;
	while (p + 8 <= d.size())
	{
		uint16_t type = U16(d, p), count = U16(d, p + 2);
		p += 8;
		if (type == 0)
			break;
		for (unsigned k = 0; k < count && p + 12 * (k + 1) <= d.size(); k++)
		{
			size_t e = p + 12 * k;
			size_t off = static_cast<size_t>(U16(d, e)) << shift, len = static_cast<size_t>(U16(d, e + 2)) << shift;
			int rid = U16(d, e + 6) & 0x7fff;
			if (type != 0x8006 || off + len > d.size() || rid == 0)
				continue;
			size_t q = off;
			for (int j = 0; j < 16 && q < off + len; j++)
			{
				unsigned n = d[q++];
				if (n && q + n <= off + len)
					Strings[(rid - 1) * 16 + j].assign(reinterpret_cast<const char*>(&d[q]), n);
				q += n;
			}
		}
		p += 12 * count;
	}
	IsLoaded = !Strings.empty();
	return IsLoaded;
}

const char* ft1_strings::Get(int id)
{
	auto it = Strings.find(id);
	return it == Strings.end() ? "" : it->second.c_str();
}

bool ft1_strings::Loaded()
{
	return IsLoaded;
}
