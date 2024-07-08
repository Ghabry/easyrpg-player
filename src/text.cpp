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
#include <lcf/data.h>
#include "cache.h"
#include "directory_tree.h"
#include "output.h"
#include "utils.h"
#include "bitmap.h"
#include "font.h"
#include "text.h"
#include "compiler.h"

#include <cctype>
#include <iterator>
#include <lcf/string_view.h>
#include <unicode/utypes.h>

Point Text::Draw(Bitmap& dest, int x, int y, const Font& font, const Bitmap& system, int color, char32_t glyph, bool is_exfont) {
	if (is_exfont) {
		if (!font.IsStyleApplied()) {
			return Font::exfont->Render(dest, x, y, system, color, glyph);
		} else {
			auto style = font.GetCurrentStyle();
			auto style_guard = Font::exfont->ApplyStyle(style);
			return Font::exfont->Render(dest, x, y, system, color, glyph);
		}
	} else {
		return font.Render(dest, x, y, system, color, glyph);
	}
}

Point Text::Draw(Bitmap& dest, int x, int y, const Font& font, Color color, char32_t glyph, bool is_exfont) {
	if (is_exfont) {
		if (!font.IsStyleApplied()) {
			return Font::exfont->Render(dest, x, y, color, glyph);
		} else {
			auto style = font.GetCurrentStyle();
			auto style_guard = Font::exfont->ApplyStyle(style);
			return Font::exfont->Render(dest, x, y, color, glyph);
		}
	} else {
		return font.Render(dest, x, y, color, glyph);
	}
}

Point Text::Draw(Bitmap& dest, const int x, const int y, const Font& font, const Bitmap& system, const int color, StringView text, const Text::Alignment align) {
	if (text.length() == 0) return { 0, 0 };

	Rect dst_rect = Text::GetSize(font, text);

	const int ih = dst_rect.height;

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

	const int iy = dst_rect.y;
	const int ix = dst_rect.x;

	// Where to draw the next glyph (x pos)
	int next_glyph_pos = 0;

	// This loops always renders a single char, color blends it and then puts
	// it onto the text_surface (including the drop shadow)
	if (font.CanShape()) {
		auto runs = Bidi(text, RTL);

		// Collect all glyphs until ExFont or end of string and then shape and render
		for (const auto& run: runs) {
			if (runs.size() > 1) {
				Output::Debug("{} {}", run.text, (int)run.direction);
			}
			auto iter = run.text.data();
			const auto end = iter + run.text.size();
			std::u32string text32;
			while (iter != end) {
				auto ret = Utils::TextNext(iter, end, 0);

				iter = ret.next;
				if (EP_UNLIKELY(!ret)) {
					continue;
				}

				if (EP_UNLIKELY(Utils::IsControlCharacter(ret.ch))) {
					next_glyph_pos += Draw(dest, ix + next_glyph_pos, iy, font, system, color, ret.ch, ret.is_exfont).x;
					continue;
				}

				if (ret.is_exfont) {
					if (!text32.empty()) {
						auto shape_ret = font.Shape(text32, run.direction);
						text32.clear();

						for (const auto& ch: shape_ret) {
							next_glyph_pos += font.Render(dest, ix + next_glyph_pos, iy, system, color, ch).x;
						}
					}

					next_glyph_pos += Draw(dest, ix + next_glyph_pos, iy, font, system, color, ret.ch, true).x;
					continue;
				}

				text32 += ret.ch;
			}

			if (!text32.empty()) {
				auto shape_ret = font.Shape(text32, run.direction);

				for (const auto& ch: shape_ret) {
					next_glyph_pos += font.Render(dest, ix + next_glyph_pos, iy, system, color, ch).x;
				}
			}
		}
	} else {
		auto iter = text.data();
		const auto end = iter + text.size();
		while (iter != end) {
			auto ret = Utils::TextNext(iter, end, 0);

			iter = ret.next;
			if (EP_UNLIKELY(!ret)) {
				continue;
			}
			next_glyph_pos += Text::Draw(dest, ix + next_glyph_pos, iy, font, system, color, ret.ch, ret.is_exfont).x;
		}
	}
	return { next_glyph_pos, ih };
}

Point Text::Draw(Bitmap& dest, const int x, const int y, const Font& font, const Color color, StringView text) {
	if (text.length() == 0) return { 0, 0 };

	int dx = x;
	int mx = x;

	int dy = y;
	int ny = 0;

	auto iter = text.data();
	const auto end = iter + text.size();
	while (iter != end) {
		auto ret = Utils::UTF8Next(iter, end);

		iter = ret.next;
		if (EP_UNLIKELY(!ret)) {
			continue;
		}

		if (ret.ch == U'\n') {
			if (ny == 0) {
				ny = font.GetSize(ret.ch).height;
			}
			dy += ny;
			mx = std::max(mx, dx);
			dx = x;
			ny = 0;
			continue;
		}

		auto rect = font.Render(dest, dx, dy, color, ret.ch);
		dx += rect.x;
		assert(ny == 0 || ny == rect.y);
		ny = rect.y;
	}
	dy += ny;
	mx = std::max(mx, dx);

	return { mx - x , dy - y };
}

Rect Text::GetSize(const Font& font, StringView text) {
	Rect rect;
	Rect rect_tmp;

	auto iter = text.data();
	const auto end = iter + text.size();

	if (font.CanShape()) {
		std::u32string text32;
		while (iter != end) {
			auto ret = Utils::TextNext(iter, end, 0);

			iter = ret.next;
			if (EP_UNLIKELY(!ret)) {
				continue;
			}

			if (EP_UNLIKELY(Utils::IsControlCharacter(ret.ch))) {
				rect_tmp = GetSize(font, ret.ch, ret.is_exfont);
				rect.width += rect_tmp.width;
				rect.height = std::max(rect.height, rect_tmp.height);
				continue;
			}

			if (ret.is_exfont) {
				if (!text32.empty()) {
					auto shape_ret = font.Shape(text32, Direction::LTR);
					text32.clear();

					for (const auto& ch: shape_ret) {
						Rect size = font.GetSize(ch);
						rect.width += ch.offset.x + size.width;
						rect.height = std::max(rect.height, size.height);
					}
				}

				rect_tmp = GetSize(font, ret.ch, ret.is_exfont);
				rect.width += rect_tmp.width;
				rect.height = std::max(rect.height, rect_tmp.height);
				continue;
			}

			text32 += ret.ch;
		}

		if (!text32.empty()) {
			auto shape_ret = font.Shape(text32, Direction::LTR);

			for (const auto& ch: shape_ret) {
				Rect size = font.GetSize(ch);
				rect.width += ch.offset.x + size.width;
				rect.height = std::max(rect.height, size.height);
			}
		}
	} else {
		while (iter != end) {
			auto ret = Utils::TextNext(iter, end, 0);

			iter = ret.next;
			if (EP_UNLIKELY(!ret)) {
				continue;
			}

			rect_tmp = GetSize(font, ret.ch, ret.is_exfont);
			rect.width += rect_tmp.width;
			rect.height = std::max(rect.height, rect_tmp.height);
		}
	}

	return rect;
}

Rect Text::GetSize(const Font& font, char32_t glyph, bool is_exfont) {
	if (is_exfont) {
		if (!font.IsStyleApplied()) {
			return Font::exfont->GetSize(glyph);
		} else {
			auto style = font.GetCurrentStyle();
			auto style_guard = Font::exfont->ApplyStyle(style);
			return Font::exfont->GetSize(glyph);
		}
	} else {
		return font.GetSize(glyph);
	}
}

#include <unicode/ubidi.h>
#include <unicode/utypes.h>

std::vector<Text::Run> Text::Bidi(StringView text, Text::Direction text_direction) {
	UErrorCode error_code = U_ZERO_ERROR;

	if (U_FAILURE(error_code) || text.empty()) {
		return {};
	}

	auto text16 = Utils::DecodeUTF16(text);

	UBiDi* bidi = ubidi_openSized(text16.size(), 0, &error_code);
	if (bidi == NULL) {
		return {};
	}

	ubidi_setPara(bidi, text16.c_str(), text16.size(),
		text_direction == 0 ? UBIDI_DEFAULT_LTR : UBIDI_DEFAULT_RTL, nullptr, &error_code);

	if (U_FAILURE(error_code)) {
		ubidi_close(bidi);
		return {};
	}

	UBiDiDirection direction = ubidi_getDirection(bidi);
	std::vector<Run> runs;

	if (direction != UBIDI_MIXED) {
		// unidirectional
		runs.push_back({ToString(text), static_cast<Direction>(direction)});
	} else {
		// mixed-directional
		int32_t count = ubidi_countRuns(bidi, &error_code);

		if (U_FAILURE(error_code)) {
			ubidi_close(bidi);
			return {};
		}

		int32_t start, length;
		std::string subtext;
		std::u32string subtext32;

		// iterate over directional runs
		for (int32_t i = 0; i < count; ++i) {
			direction = ubidi_getVisualRun(bidi, i, &start, &length);

			// FIXME: Doing UTF16-8-32 conversion, better would be a UTF16-32 conversion
			subtext = Utils::EncodeUTF(std::u16string_view(text16.data() + start, length));
			//subtext32 = Utils::DecodeUTF32(subtext);

			runs.push_back({subtext, static_cast<Direction>(direction)});
		}

	}

	ubidi_close(bidi);

	return runs;
}
