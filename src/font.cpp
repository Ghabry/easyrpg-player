/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

// Headers
#include <map>
#include <vector>

#include <boost/next_prior.hpp>
#include <boost/regex/pending/unicode_iterator.hpp>
#include <boost/type_traits/remove_pointer.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_BITMAP_H
#include FT_MODULE_H

#include "reader_util.h"
#include "shinonome.hxx"

#include "filefinder.h"
#include "output.h"
#include "font.h"
#include "bitmap.h"
#include "utils.h"
#include "cache.h"
#include "metrics.h"

bool operator<(ShinonomeGlyph const& lhs, uint32_t const code) {
	return lhs.code < code;
}

// Static variables.
namespace {
	typedef std::map<std::string, EASYRPG_WEAK_PTR<boost::remove_pointer<FT_Face>::type> > face_cache_type;
	face_cache_type face_cache;
	ShinonomeGlyph const* find_glyph(ShinonomeGlyph const* data, size_t size, uint32_t code) {
		ShinonomeGlyph const* ret = std::lower_bound(data, data + size, code);
		if(ret != (data + size) && ret->code == code) {
			return ret;
		} else {
			static ShinonomeGlyph const empty_glyph = { 0, true, {0} };
			Output::Debug("glyph not found: 0x%04x", code);
			return &empty_glyph;
		}
	}

	ShinonomeGlyph const* find_gothic_glyph(uint32_t code) {
		return find_glyph(SHINONOME_GOTHIC,
						  sizeof(SHINONOME_GOTHIC) / sizeof(ShinonomeGlyph), code);
	}

	ShinonomeGlyph const* find_mincho_glyph(uint32_t code) {
		ShinonomeGlyph const* const mincho =
			find_glyph(SHINONOME_MINCHO,
					   sizeof(SHINONOME_MINCHO) / sizeof(ShinonomeGlyph), code);
		return mincho == NULL? find_gothic_glyph(code) : mincho;
	}

	struct ShinonomeFont : public Font {
		enum { HEIGHT = 12, FULL_WIDTH = HEIGHT, HALF_WIDTH = FULL_WIDTH / 2 };

		typedef ShinonomeGlyph const*(*function_type)(uint32_t);

		ShinonomeFont(function_type func);

		Rect GetSize(std::string const& txt);

		BitmapRef Glyph(unsigned code, Rect& glyph_box);

	private:
		function_type const func_;
	}; // class ShinonomeFont


	void delete_face(FT_Face f) {
		if(FT_Done_Face(f) != FT_Err_Ok) {
			Output::Warning("FT_Face deleting error.");
		}
	}

	void delete_library(FT_Library f) {
		if(FT_Done_Library(f) != FT_Err_Ok) {
			Output::Warning("FT_Library deleting error.");
		}
	}

	struct FTFont : public Font  {
		FTFont(const std::string& name, int size, bool bold, bool italic);

		Rect GetSize(std::string const& txt);

		BitmapRef Glyph(unsigned code, Rect& glyph_box);

	private:
		static EASYRPG_WEAK_PTR<boost::remove_pointer<FT_Library>::type> library_checker_;
		EASYRPG_SHARED_PTR<boost::remove_pointer<FT_Library>::type> library_;
		EASYRPG_SHARED_PTR<boost::remove_pointer<FT_Face>::type> face_;
		std::string face_name_;
		unsigned current_size_;

		bool check_face();
	}; // class FTFont

	FontRef const gothic = EASYRPG_MAKE_SHARED<ShinonomeFont>(&find_gothic_glyph);
	FontRef const mincho = EASYRPG_MAKE_SHARED<ShinonomeFont>(&find_mincho_glyph);
	FontRef const freetype = std::make_shared<FTFont>("Microsoft Sans Serif", 10, false, false);

	struct ExFont : public Font {
		ExFont();
		Rect GetSize(std::string const& txt);
		BitmapRef Glyph(unsigned code, Rect& glyph_box);
	};
} // anonymous namespace

ShinonomeFont::ShinonomeFont(ShinonomeFont::function_type func)
	: Font("Shinonome", HEIGHT, false, false), func_(func) {}

Rect ShinonomeFont::GetSize(std::string const& txt) {
	typedef boost::u8_to_u32_iterator<std::string::const_iterator> iterator;
	size_t units = 0;
	iterator i(txt.begin(), txt.begin(), txt.end());
	iterator const end(txt.end(), txt.begin(), txt.end());
	for(; i != end; ++i) {
		ShinonomeGlyph const* const glyph = func_(*i);
		assert(glyph);
		units += glyph->is_full? 2 : 1;
	}
	return Rect(0, 0, units * HALF_WIDTH, HEIGHT);
}

BitmapRef ShinonomeFont::Glyph(unsigned code, Rect& glyph_box) {
	ShinonomeGlyph const* const glyph = func_(code);
	assert(glyph);
	size_t const width = glyph->is_full? FULL_WIDTH : HALF_WIDTH;

	BitmapRef bm = Bitmap::Create(reinterpret_cast<void*>(NULL), width, HEIGHT, 0, DynamicFormat(8,8,0,8,0,8,0,8,0,PF::Alpha));
	uint8_t* data = reinterpret_cast<uint8_t*>(bm->pixels());
	int pitch = bm->pitch();
	for(size_t y_ = 0; y_ < HEIGHT; ++y_)
		for(size_t x_ = 0; x_ < width; ++x_)
			data[y_*pitch+x_] = (glyph->data[y_] & (0x1 << x_)) ? 255 : 0;

	glyph_box.Set(0, 0, width, HEIGHT);

	return bm;
}

EASYRPG_WEAK_PTR<boost::remove_pointer<FT_Library>::type> FTFont::library_checker_;

FTFont::FTFont(const std::string& name, int size, bool bold, bool italic)
	: Font(name, size, bold, italic), current_size_(0) {
	
}

Rect FTFont::GetSize(std::string const& txt) {
	if (!check_face()) {
		return Font::Default()->GetSize(txt);
	}

	typedef boost::u8_to_u32_iterator<std::string::const_iterator> iterator;
	size_t units = 0;
	iterator i(txt.begin(), txt.begin(), txt.end());
	iterator const end(txt.end(), txt.begin(), txt.end());
	Rect size;
	for (; i != end; ++i) {
		if (FT_Load_Char(face_.get(), *i, FT_LOAD_NO_BITMAP) != FT_Err_Ok) {
			Output::Debug("Couldn't load FreeType character %d", *i);
		}

		if (FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_MONO) != FT_Err_Ok) {
			Output::Debug("Couldn't render FreeType character %d", *i);
		}

		FT_GlyphSlot slot = face_->glyph;
		size.width += slot->bitmap.width + Metrics::Upscale(2);
		size.height += slot->bitmap.rows;
	}

	return size;
}

BitmapRef FTFont::Glyph(unsigned glyph, Rect& glyph_box) {
	if(!check_face()) {
		return Font::Default()->Glyph(glyph, glyph_box);
	}

	if (FT_Load_Char(face_.get(), glyph, FT_LOAD_NO_BITMAP) != FT_Err_Ok) {
		Output::Error("Couldn't load FreeType character %d", glyph);
	}

    if (FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL) != FT_Err_Ok) {
		Output::Error("Couldn't render FreeType character %d", glyph);
	}

	FT_GlyphSlot slot = face_->glyph;

	FT_Bitmap const& ft_bitmap = slot->bitmap;

	size_t const pitch = std::abs(ft_bitmap.pitch);
	int const width = ft_bitmap.width;
	int const height = ft_bitmap.rows;
	int const top_offset = slot->bitmap_top;
	int const left_offset = slot->bitmap_left;


	BitmapRef bm = Bitmap::Create(width, height);
	uint32_t* data = reinterpret_cast<uint32_t*>(bm->pixels());
	//Output::Debug("%c x%d y%d w%d h%d", (char)glyph, slot->bitmap_left, slot->bitmap_top, width, height);
	for(int row = 0; row < height; ++row) {
		for(int col = 0; col < width; ++col) {
			unsigned c = ft_bitmap.buffer[pitch * row + col];
			uint32_t pixel = (c << 24) + (c << 16) + (c << 8) + c;
			data[row * width + col] = pixel;
		}
	}

	glyph_box.Set(left_offset, Metrics::Upscale(12) - top_offset, width, height);

	return bm;
}

FontRef Font::Default(bool const m) {
	return freetype;
}

FontRef Font::Create(const std::string& name, int size, bool bold, bool italic) {
	return EASYRPG_MAKE_SHARED<FTFont>(name, size, bold, italic);
}

void Font::Dispose() {
	for(face_cache_type::const_iterator i = face_cache.begin(); i != face_cache.end(); ++i) {
		if(i->second.expired()) { continue; }
		Output::Debug("possible leak in cached font face %s", i->first.c_str());
	}
	face_cache.clear();
}

// Constructor.
Font::Font(const std::string& name, int size, bool bold, bool italic)
	: name(name)
	, size(size)
	, bold(bold)
	, italic(italic)
{
}

bool FTFont::check_face() {
	if(!library_) {
		if(library_checker_.expired()) {
			FT_Library lib;
			if(FT_Init_FreeType(&lib) != FT_Err_Ok) {
				Output::Error("Couldn't initialize FreeType");
				return false;
			}
			library_.reset(lib, delete_library);
			library_checker_ = library_;
		} else {
			library_ = library_checker_.lock();
		}
	}

	if(!face_ || face_name_ != name) {
	    face_cache_type::const_iterator it = face_cache.find(name);
		if(it == face_cache.end() || it->second.expired()) {
			std::string const face_path = FileFinder::FindFont(name);
			FT_Face face;
			if(FT_New_Face(library_.get(), face_path.c_str(), 0, &face) != FT_Err_Ok) {
				Output::Error("Couldn't initialize FreeType face: %s(%s)",
							  name.c_str(), face_path.c_str());
				return false;
			}

			face_.reset(face, delete_face);

			for (int i = 0; i < face_->num_fixed_sizes; i++) {
				FT_Bitmap_Size* size = &face_->available_sizes[i];
				Output::Debug("Font Size %d: %d %d %f %f %f", i,
					size->width, size->height, size->size / 64.0,
					size->x_ppem / 64.0, size->y_ppem / 64.0);
			}

			face_cache[name] = face_;
		} else {
			face_ = it->second.lock();
		}
		face_name_ = name;
	};

	face_->style_flags =
		(bold? FT_STYLE_FLAG_BOLD : 0) |
		(italic? FT_STYLE_FLAG_ITALIC : 0);

	if(current_size_ != size) {
		if (FT_Set_Pixel_Sizes(face_.get(), Metrics::Upscale(12), Metrics::Upscale(12)) != FT_Err_Ok) {
			Output::Error("Couldn't set FreeType face size");
			return false;
		}
		current_size_ = size;
	}

	return true;
}

void Font::Render(Bitmap& bmp, int const x, int const y, Bitmap const& sys, int color, unsigned code) {
	if(color != ColorShadow) {
		BitmapRef system = Cache::System();
		Render(bmp, x + Metrics::Upscale(1), y + Metrics::Upscale(1), system->GetShadowColor(), code);
	}

	Rect glyph_box;
	BitmapRef bm = Glyph(code, glyph_box);
	Output::Debug("xy %d %d", glyph_box.x, glyph_box.y);
	glyph_box.x += x;
	glyph_box.y += y;
	Output::Debug("x2 %d %d", glyph_box.x, glyph_box.y);
	unsigned const
		src_x = color == ColorShadow? 16 : color % 10 * 16 + 2,
		src_y = color == ColorShadow? 32 : color / 10 * 16 + 48 + 16;

	bmp.MaskedBlit(glyph_box, *bm, 0, 0, sys, Metrics::Upscale(src_x), Metrics::Upscale(src_y));
}

void Font::Render(Bitmap& bmp, int x, int y, Color const& color, unsigned code) {
	Rect glyph_box;
	BitmapRef bm = Glyph(code, glyph_box);
	glyph_box.x += x;
	glyph_box.y += y;

	bmp.MaskedBlit(glyph_box, *bm, 0, 0, color);
}

ExFont::ExFont() : Font("exfont", 12, false, false) {
}

FontRef Font::exfont = EASYRPG_MAKE_SHARED<ExFont>();

BitmapRef ExFont::Glyph(unsigned code, Rect& glyph_box) {
	BitmapRef exfont = Cache::Exfont();
	Rect const rect((code % 13) * 12, (code / 13) * 12, 12, 12);
	glyph_box.Set(0, 0, 12, 12);
	return Bitmap::Create(*exfont, rect, true);
}

Rect ExFont::GetSize(std::string const& /* txt */) {
	return Rect(0, 0, 12, 12);
}

