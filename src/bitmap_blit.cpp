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

#include "bitmap_blit.h"
#include <pixman.h>

namespace {

bool adjust_rects(Bitmap const& dest, Rect& dst_rect, Bitmap const& src, Rect& src_rect, Opacity const& opacity) {
	if (!Rect::AdjustRectangles(src_rect, dst_rect, src.GetRect()))
		return false;

	if (!Rect::AdjustRectangles(dst_rect, src_rect, dest.GetRect()))
		return false;

	return true;
}

}

namespace BitmapBlit {

bool Blit(Bitmap& dest, int x, int y, Bitmap const& src, Rect src_rect,
		Opacity const& opacity, pixman_op_t blend_mode) {

	if (blend_mode == PIXMAN_OP_SRC) {
		return BlitFast(dest, x, y, src, src_rect, opacity);
	}

	if (opacity.IsTransparent()) {
		return true;
	}

	if (dest.format != src.format) {
		return false;
	}

	if (blend_mode != PIXMAN_OP_OVER || src.bpp() != 2) {
		// Not supported
		return false;
	}

	Rect dst_rect = {x, y, 0, 0};

	if (!adjust_rects(dest, dst_rect, src, src_rect, opacity)) {
		return true;
	}

	int bpp = src.bpp();
	int src_pitch = src.pitch();
	int dst_pitch = dest.pitch();

	uint8_t* src_pixels = (uint8_t*)src.pixels() + src_rect.x * bpp + src_rect.y * src_pitch;
	uint8_t* dst_pixels = (uint8_t*)dest.pixels() + dst_rect.x * bpp + dst_rect.y * dst_pitch;
	uint16_t src_pixel;

	int to_copy = src_rect.width * bpp;

	int a_mask = src.format.a.mask;
	int run_beg = 0;
	int run_alpha = 0;
	int pix_alpha = 0;

	for (y = 0; y < src_rect.height; ++y) {
		run_beg = 0;
		src_pixel = *(uint16_t*)(src_pixels + y * src_pitch + 0);
		run_alpha = (src_pixel & a_mask);

		for (x = 0; x < src_rect.width * bpp; x += bpp) {
			src_pixel = *(uint16_t*)(src_pixels + y * src_pitch + x);
			pix_alpha = (src_pixel & a_mask);

			if (pix_alpha != run_alpha) {
				if (run_alpha != 0) {
					memcpy(
						dst_pixels + y * dst_pitch + run_beg,
						src_pixels + y * src_pitch + run_beg,
						x - run_beg);
				}

				run_beg = x;
				run_alpha = pix_alpha;
			}
		}

		if (run_alpha != 0) {
			if (run_beg == 0) {
				// Copy entire line
				memcpy(
					dst_pixels + y * dst_pitch,
					src_pixels + y * src_pitch,
					src_rect.width * bpp);
			} else {
				// Copy remainder
				memcpy(
					dst_pixels + y * dst_pitch + run_beg,
					src_pixels + y * src_pitch + run_beg,
					x - bpp - run_beg);
			}
		}
	}

	/*
	// Naive implementation
	for (int y = 0; y < src_rect.height; ++y) {
		for (int x = 0; x < src_rect.width * bpp; x += bpp) {
			src_pixel = *(uint16_t*)(src_pixels + y * src_pitch + x);

			if ((src_pixel & a_mask) != 0) {
				*(uint16_t*)(dst_pixels + y * dst_pitch + x) = src_pixel;
			}
		}
	}
	*/

	return true;
}

bool BlitFast(Bitmap& dest, int x, int y, Bitmap const& src, Rect src_rect,
		Opacity const& opacity) {

	if (opacity.IsTransparent()) {
		return true;
	}

	if (dest.format != src.format) {
		return false;
	}

	Rect dst_rect = {x, y, 0, 0};

	if (!adjust_rects(dest, dst_rect, src, src_rect, opacity)) {
		return true;
	}

	int bpp = src.bpp();
	int src_pitch = src.pitch();
	int dst_pitch = dest.pitch();

	uint8_t* src_pixels = (uint8_t*)src.pixels() + src_rect.x * bpp + src_rect.y * src_pitch;
	uint8_t* dst_pixels = (uint8_t*)dest.pixels() + dst_rect.x * bpp + dst_rect.y * dst_pitch;

	int to_copy = src_rect.width * bpp;

	for (y = 0; y < src_rect.height; ++y) {
		memcpy(
			dst_pixels + y * dst_pitch,
			src_pixels + y * src_pitch,
			to_copy);
	}

	return true;
}

} // namespace BitmapBlit
