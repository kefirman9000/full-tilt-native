// Marble Drop marbles as ball styles, see marble_balls.h
#include "pch.h"
#include "marble_balls.h"

#include "gdrv.h"
#include "pb.h"

#include <cmath>
#include <fstream>
#include <map>
#include <memory>

namespace marbles
{
	namespace
	{
		// The solid marbles in the "marbles" group, by frame; odd frames are the dithered "ghost" versions and
		// frames 20+ are bursts, sparkles and squashes, none of which make sense as a ball.
		struct MarbleColor
		{
			const char* Style;
			const char* Name;
			int Frame;
		};

		const MarbleColor Colors[] =
		{
			{"marble_red", "Red", 0},
			{"marble_yellow", "Yellow", 2},
			{"marble_blue", "Blue", 4},
			{"marble_orange", "Orange", 6},
			{"marble_green", "Green", 8},
			{"marble_magenta", "Magenta", 10},
			{"marble_black", "Black", 12},
			{"marble_steel", "Steel", 14},
			{"marble_glass", "Glass", 16},
			{"marble_fire", "Fire", 18},
		};

		struct Image
		{
			int W = 0, H = 0;
			std::vector<float> Rgba; // premultiplied, top-down
		};

		bool Loaded = false;
		std::vector<std::string> StyleList;
		std::map<int, Image> Marbles; // by frame
		std::map<std::string, std::vector<std::unique_ptr<gdrv_bitmap8>>> Cache;

		std::vector<uint8_t> ReadFile(const std::string& path)
		{
			std::ifstream in(path, std::ios::binary);
			if (!in)
				return {};
			return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		}

		std::vector<std::string> Candidates()
		{
			std::vector<std::string> dirs;
			if (auto env = getenv("FT_MARBLEDROP"))
				dirs.push_back(std::string(env) + PathSeparator);
			// The engine runs in tables/<Table>/; the launcher stores Marble Drop in tables/_MarbleDrop
			dirs.push_back(pb::make_path_name(std::string("..") + PathSeparator + "_MarbleDrop" + PathSeparator));
			dirs.push_back(pb::make_path_name(std::string("_MarbleDrop") + PathSeparator));
			if (auto exe = SDL_GetBasePath())
			{
				dirs.push_back(std::string(exe) + "tables" + PathSeparator + "_MarbleDrop" + PathSeparator);
				SDL_free(exe);
			}
			std::vector<std::string> files;
			// COREB has the bigger marbles (14x15 vs 11x11), which scale better
			for (auto& dir : dirs)
				for (auto name : {"COREB.DAT", "coreb.dat", "COREA.DAT", "corea.dat"})
					files.push_back(dir + name);
			return files;
		}

		// PARTOUT(4.0)RESOURCE: 183-byte header, then groups of (type, [size], data) entries; type 0 is an inline short
		struct Entry
		{
			uint8_t Type;
			const uint8_t* Data;
			uint32_t Size;
		};

		bool ParseGroups(const std::vector<uint8_t>& d, std::vector<std::vector<Entry>>& groups)
		{
			if (d.size() < 183 || std::memcmp(d.data(), "PARTOUT", 7) != 0)
				return false;
			uint16_t count;
			std::memcpy(&count, &d[175], 2);
			size_t off = 183;
			for (int g = 0; g < count; g++)
			{
				if (off >= d.size())
					return false;
				int n = d[off++];
				std::vector<Entry> entries;
				for (int e = 0; e < n; e++)
				{
					if (off >= d.size())
						return false;
					auto type = d[off++];
					if (type == 0)
					{
						if (off + 2 > d.size())
							return false;
						entries.push_back({type, &d[off], 2});
						off += 2;
						continue;
					}
					if (off + 4 > d.size())
						return false;
					uint32_t size;
					std::memcpy(&size, &d[off], 4);
					off += 4;
					if (off + size > d.size())
						return false;
					entries.push_back({type, &d[off], size});
					off += size;
				}
				groups.push_back(std::move(entries));
			}
			return true;
		}

		bool LoadFile(const std::string& path)
		{
			auto data = ReadFile(path);
			std::vector<std::vector<Entry>> groups;
			if (data.empty() || !ParseGroups(data, groups))
				return false;

			// Palette: like gdrv::display_palette, 0 is transparent and 1-9 / 246-255 are the Windows system colours
			const uint32_t sys[20] =
			{
				0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0, 0xC0DCC0, 0xA6CAF0,
				0xFFFBF0, 0xA0A0A4, 0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
			};
			uint32_t palette[256]{};
			for (int i = 0; i < 10; i++)
				palette[i] = sys[i];
			for (int i = 246; i < 256; i++)
				palette[i] = sys[10 + i - 246];
			const uint8_t* plt = nullptr;
			for (auto& g : groups)
				for (auto& e : g)
					if (e.Type == 5 && e.Size >= 1024 && !plt)
						plt = e.Data;
			if (!plt)
				return false;
			for (int i = 10; i < 246; i++)
				palette[i] = plt[i * 4 + 2] << 16 | plt[i * 4 + 1] << 8 | plt[i * 4]; // stored B, G, R, x

			int first = -1, count = 0;
			for (size_t g = 0; g < groups.size() && first < 0; g++)
				for (auto& e : groups[g])
					if (e.Type == 3 && std::strncmp(reinterpret_cast<const char*>(e.Data), "marbles", e.Size) == 0)
					{
						first = static_cast<int>(g);
						for (auto& a : groups[g])
							if (a.Type == 10 && a.Size >= 4)
							{
								int16_t v[2];
								std::memcpy(v, a.Data, 4);
								if (v[0] == 100)
									count = v[1];
							}
					}
			if (first < 0 || count <= 0)
				return false;

			for (auto& c : Colors)
			{
				if (c.Frame >= count || first + c.Frame >= static_cast<int>(groups.size()))
					continue;
				for (auto& e : groups[first + c.Frame])
				{
					if (e.Type != 1 || e.Size < 14)
						continue;
					int16_t w, h;
					std::memcpy(&w, e.Data + 1, 2);
					std::memcpy(&h, e.Data + 3, 2);
					uint32_t size;
					std::memcpy(&size, e.Data + 9, 4);
					auto flags = e.Data[13];
					if (w <= 0 || h <= 0 || (flags & 4) || size + 14 > e.Size)
						break; // spliced or broken: not a plain sprite
					auto stride = static_cast<int>(size / h);
					if (stride < w)
						break;
					Image img;
					img.W = w;
					img.H = h;
					img.Rgba.resize(w * h * 4);
					auto pixels = e.Data + 14;
					for (int y = 0; y < h; y++)
						for (int x = 0; x < w; x++)
						{
							auto index = pixels[(h - 1 - y) * stride + x]; // rows are stored bottom-up
							auto dst = &img.Rgba[(y * w + x) * 4];
							if (index == 0)
								continue;
							auto rgb = palette[index];
							dst[0] = ((rgb >> 16) & 0xff) / 255.0f;
							dst[1] = ((rgb >> 8) & 0xff) / 255.0f;
							dst[2] = (rgb & 0xff) / 255.0f;
							dst[3] = 1.0f;
						}
					Marbles[c.Frame] = std::move(img);
					break;
				}
			}
			return !Marbles.empty();
		}

		void Load()
		{
			if (Loaded)
				return;
			Loaded = true;
			for (auto& path : Candidates())
			{
				if (LoadFile(path))
				{
					printf("Marble Drop marbles: %s\n", path.c_str());
					break;
				}
			}
			for (auto& c : Colors)
				if (Marbles.count(c.Frame))
					StyleList.emplace_back(c.Style);
		}

		const MarbleColor* Find(const std::string& style)
		{
			for (auto& c : Colors)
				if (style == c.Style)
					return &c;
			return nullptr;
		}

		struct Box
		{
			int X0, Y0, X1, Y1; // inclusive-exclusive
		};

		Box OpaqueBox(const Image& img)
		{
			Box b{img.W, img.H, 0, 0};
			for (int y = 0; y < img.H; y++)
				for (int x = 0; x < img.W; x++)
					if (img.Rgba[(y * img.W + x) * 4 + 3] > 0)
					{
						b.X0 = std::min(b.X0, x);
						b.Y0 = std::min(b.Y0, y);
						b.X1 = std::max(b.X1, x + 1);
						b.Y1 = std::max(b.Y1, y + 1);
					}
			if (b.X1 <= b.X0)
				b = {0, 0, img.W, img.H};
			return b;
		}

		Box OpaqueBox(const gdrv_bitmap8& bmp)
		{
			Box b{bmp.Width, bmp.Height, 0, 0};
			for (int y = 0; y < bmp.Height; y++)
				for (int x = 0; x < bmp.Width; x++)
					if (bmp.BmpBufPtr1[y * bmp.Stride + x].Color)
					{
						b.X0 = std::min(b.X0, x);
						b.Y0 = std::min(b.Y0, y);
						b.X1 = std::max(b.X1, x + 1);
						b.Y1 = std::max(b.Y1, y + 1);
					}
			if (b.X1 <= b.X0)
				b = {0, 0, bmp.Width, bmp.Height};
			return b;
		}

		void Sample(const Image& img, float x, float y, float out[4])
		{
			// Bilinear on premultiplied colour; outside the image is transparent
			x -= 0.5f;
			y -= 0.5f;
			auto x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
			auto fx = x - x0, fy = y - y0;
			for (int c = 0; c < 4; c++)
				out[c] = 0;
			for (int j = 0; j < 2; j++)
				for (int i = 0; i < 2; i++)
				{
					int px = x0 + i, py = y0 + j;
					if (px < 0 || py < 0 || px >= img.W || py >= img.H)
						continue;
					auto wgt = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
					auto src = &img.Rgba[(py * img.W + px) * 4];
					for (int c = 0; c < 4; c++)
						out[c] += src[c] * wgt;
				}
		}

		gdrv_bitmap8* MakeFrame(const Image& marble, const gdrv_bitmap8& standard)
		{
			auto bmp = new gdrv_bitmap8(standard.Width, standard.Height, false, true);
			bmp->XPosition = standard.XPosition;
			bmp->YPosition = standard.YPosition;
			bmp->Resolution = standard.Resolution;
			for (int i = 0; i < bmp->Width * bmp->Height; i++)
				bmp->BmpBufPtr1[i] = ColorRgba{0};

			// Fit the marble's round part into the standard ball's round part
			auto dst = OpaqueBox(standard);
			auto src = OpaqueBox(marble);
			auto scaleX = static_cast<float>(src.X1 - src.X0) / (dst.X1 - dst.X0);
			auto scaleY = static_cast<float>(src.Y1 - src.Y0) / (dst.Y1 - dst.Y0);
			const int n = 4; // 4x4 samples per pixel: a box filter when shrinking, smooth when enlarging
			for (int y = dst.Y0; y < dst.Y1; y++)
				for (int x = dst.X0; x < dst.X1; x++)
				{
					float acc[4]{};
					for (int sy = 0; sy < n; sy++)
						for (int sx = 0; sx < n; sx++)
						{
							auto ux = src.X0 + (x - dst.X0 + (sx + 0.5f) / n) * scaleX;
							auto uy = src.Y0 + (y - dst.Y0 + (sy + 0.5f) / n) * scaleY;
							float s[4];
							Sample(marble, ux, uy, s);
							for (int c = 0; c < 4; c++)
								acc[c] += s[c];
						}
					auto a = acc[3] / (n * n);
					// The ball is drawn without blending: a pixel is either the marble or see-through.
					// Keep it where the standard ball is also solid, so the outline matches the table's ball.
					if (a < 0.5f || !standard.BmpBufPtr1[y * standard.Stride + x].Color)
						continue;
					auto r = static_cast<uint8_t>(std::min(255.0f, acc[0] / acc[3] * 255.0f + 0.5f));
					auto g = static_cast<uint8_t>(std::min(255.0f, acc[1] / acc[3] * 255.0f + 0.5f));
					auto b = static_cast<uint8_t>(std::min(255.0f, acc[2] / acc[3] * 255.0f + 0.5f));
					bmp->BmpBufPtr1[y * bmp->Stride + x] = ColorRgba{r, g, b, 0xff};
				}
			return bmp;
		}
	}

	const std::vector<std::string>& Styles()
	{
		Load();
		return StyleList;
	}

	bool IsMarbleStyle(const std::string& style)
	{
		return style.compare(0, 7, "marble_") == 0;
	}

	std::string Label(const std::string& style)
	{
		auto c = Find(style);
		return std::string("Marble Drop: ") + (c ? c->Name : style.c_str());
	}

	bool Build(const std::string& style, const std::vector<gdrv_bitmap8*>& standardFrames,
	           std::vector<gdrv_bitmap8*>& out)
	{
		Load();
		auto c = Find(style);
		if (!c || !Marbles.count(c->Frame) || standardFrames.empty())
			return false;
		// Cached per style and frame sizes (the table's resolution decides the ball sizes)
		auto key = style;
		for (auto f : standardFrames)
			key += "/" + std::to_string(f ? f->Width : 0) + "x" + std::to_string(f ? f->Height : 0);
		auto& cached = Cache[key];
		if (cached.empty())
			for (auto f : standardFrames)
			{
				if (!f || !f->BmpBufPtr1 || f->Width <= 0 || f->Height <= 0)
				{
					Cache.erase(key);
					return false;
				}
				cached.emplace_back(MakeFrame(Marbles[c->Frame], *f));
			}
		out.clear();
		for (auto& f : cached)
			out.push_back(f.get());
		return true;
	}
}
