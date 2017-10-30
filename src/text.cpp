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
#include "data.h"
#include "cache.h"
#include "output.h"
#include "utils.h"
#include "bitmap.h"
#include "font.h"
#include "text.h"
#include "game_system.h"

#include <cctype>
#include <iterator>

void Text::Draw(Bitmap& dest, int x, int y, int color, FontRef font, std::string const& text, Text::Alignment align) {
	if (text.length() == 0) return;

	Rect dst_rect = font->GetSize(text);
	dst_rect.width += 5;

	switch (align) {
	case Text::AlignCenter:
		dst_rect.x = x - dst_rect.width / 2; break;
	case Text::AlignRight:
		dst_rect.x = x - dst_rect.width; break;
	case Text::AlignLeft:
		dst_rect.x = x; break;
	default: assert(false);
	}

	dst_rect.y = y;
	dst_rect.width += 1; dst_rect.height += 1; // Need place for shadow
	if (dst_rect.IsOutOfBounds(dest.GetWidth(), dest.GetHeight())) return;

	BitmapRef text_surface; // Complete text will be on this surface
	text_surface = Bitmap::Create(dst_rect.width, dst_rect.height, true);
	text_surface->Clear();

	BitmapRef system = Cache::System();

	// The current char is an exfont
	bool is_exfont = false;

	// This loop sends blocks of chars to the font renderer which color blends it and then puts
	// it onto the text_surface (including the drop shadow)
	// A char block ends when an ExFont is encountered or reached the end of the source string
	std::u32string u32text = Utils::DecodeUTF32(text);
	std::u32string to_render = U"";

	// Where to draw the next glyph (x pos)
	Rect next_glyph_rect(0, 0, 0, 0);

	for (auto c = u32text.begin(), end = u32text.end(); c != end; ++c) {
		char32_t const next_c = std::distance(c, end) > 1? *std::next(c) : 0;

		// ExFont-Detection: Check for A-Z or a-z behind the $
		if (*c == '$' && std::isalpha(next_c)) {
			int exfont_value = -1;
			// Calculate which exfont shall be rendered
			if (islower(next_c)) {
				exfont_value = 26 + next_c - 'a';
			} else if (isupper(next_c)) {
				exfont_value = next_c - 'A';
			} else { assert(false); }
			is_exfont = true;

			if (!to_render.empty()) {
				font->Render(*text_surface, next_glyph_rect.x, next_glyph_rect.y, *system, color, to_render);
				next_glyph_rect.x += font->GetSize(to_render).width;
				to_render.clear();
			}
			Font::exfont->Render(*text_surface, next_glyph_rect.x, next_glyph_rect.y, *system, color, exfont_value);
		} else { // Not ExFont, draw normal text
			to_render += *c;
		}

		if (is_exfont) {
			is_exfont = false;
			next_glyph_rect.x += 12;
			// Skip the next character
			++c;
		}
	}

	if (!to_render.empty()) {
		font->Render(*text_surface, next_glyph_rect.x, next_glyph_rect.y, *system, color, to_render);
	}

	BitmapRef text_bmp = Bitmap::Create(*text_surface, text_surface->GetRect());

	Rect src_rect(0, 0, dst_rect.width, dst_rect.height);
	int iy = dst_rect.y;
	if (dst_rect.height > text_bmp->GetHeight()) {
		iy += ((dst_rect.height - text_bmp->GetHeight()) / 2);
	}
	int ix = dst_rect.x;

	dest.Blit(ix, iy, *text_bmp, src_rect, 255);
}

void Text::Draw(Bitmap& dest, int x, int y, Color color, FontRef font, std::string const& text) {
	if (text.length() == 0) return;

	int next_glyph_pos = 0;

	for (char32_t c : Utils::DecodeUTF32(text)) {
		std::u32string const glyph = std::u32string(1, c);
		if (c == U'\n') {
			y += font->GetSize(glyph).height;
			next_glyph_pos = 0;
			continue;
		}
		Rect next_glyph_rect(x + next_glyph_pos, y, 0, 0);

		font->Render(dest, next_glyph_rect.x, next_glyph_rect.y, color, c);

		next_glyph_pos += font->GetSize(glyph).width;
	}
}
