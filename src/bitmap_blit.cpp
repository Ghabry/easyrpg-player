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
#include "pixel_format.h"
#include <pixman.h>

namespace {

bool AdjustRects(Bitmap const& dest, Rect& dst_rect, Bitmap const& src, Rect& src_rect, Opacity const& opacity) {
	if (!Rect::AdjustRectangles(src_rect, dst_rect, src.GetRect()))
		return false;

	if (!Rect::AdjustRectangles(dst_rect, src_rect, dest.GetRect()))
		return false;

	return true;
}

int GetMaskValue(Opacity const& opacity) {
	if (opacity.IsOpaque() || opacity.IsTransparent()) {
		return -1;
	}

	assert(!opacity.IsSplit());

	return opacity.Value();
}

template<typename FORMAT>
bool BlitT(Bitmap& dest, Rect const& dst_rect, Bitmap const& src, Rect const& src_rect,
		Opacity const& opacity) {
	int x = 0;
	int y = 0;

	int mask = GetMaskValue(opacity);

	auto format = FORMAT();
	const int bpp = FORMAT().bytes;

	int src_w = src.pitch() / bpp;
	int dst_w = dest.pitch() / bpp;

	using pixel_type = typename FORMAT::bits_traits_type::type;

	pixel_type* src_pixels = (pixel_type*)src.pixels();
	src_pixels += src_rect.x + src_rect.y * src_w;

	pixel_type* dst_pixels = (pixel_type*)dest.pixels();
	dst_pixels += dst_rect.x + dst_rect.y * dst_w;

	int src_advance = src_w - src_rect.width;
	int dst_advance = dst_w - dst_rect.width;

	const pixel_type amask = format.a_mask();

	if (mask >= 0) {
		// Alpha blending required (slow)
		const uint8_t rshift = format.r_shift();
		const uint8_t gshift = format.g_shift();
		const uint8_t bshift = format.b_shift();
		const uint8_t ashift = format.a_shift();
		const uint8_t bits = format.r_bits();
		const uint16_t pxmax = (1 << bits);
		const uint8_t pxmask = pxmax - 1;

		uint8_t rs, gs, bs; // src colors
		uint8_t rd, gd, bd; // dest colors

		mask /= (256 / pxmax); // Reduce range to [0 - 32] (5 bit)

		auto set_pixel_fn = [&](pixel_type* src_pixel, pixel_type* dst_pixel) {
			pixel_type src_p = *src_pixel;
			pixel_type dst_p = *dst_pixel;

			rs = (src_p >> rshift) & pxmask;
			gs = (src_p >> gshift) & pxmask;
			bs = (src_p >> bshift) & pxmask;

			rd = (dst_p >> rshift) & pxmask;
			gd = (dst_p >> gshift) & pxmask;
			bd = (dst_p >> bshift) & pxmask;

			rd = (rs * mask + ((pxmax - mask) * rd)) >> bits;
			gd = (gs * mask + ((pxmax - mask) * gd)) >> bits;
			bd = (bs * mask + ((pxmax - mask) * bd)) >> bits;

			*dst_pixel = (rd << rshift ) | (gd << gshift) | (bd << bshift) | (1 << ashift);
		};

		if (!src.GetTransparent()) {
			for (y = 0; y < src_rect.height; ++y) {
				for (x = 0; x < src_rect.width; ++x) {
					set_pixel_fn(src_pixels, dst_pixels);

					++src_pixels;
					++dst_pixels;
				}

				src_pixels += src_advance;
				dst_pixels += dst_advance;
			}
		} else {
			// Transparent pixels are skipped
			auto& runs = src.GetRuns();

			if (!runs.empty()) {
				auto run_it = runs.begin();

				uint16_t x_begin = src_rect.x;
				uint16_t x_end = src_rect.x + src_rect.width;

				for (int y = src_rect.y; y < src_rect.y + src_rect.height; ++y) {
					for (; run_it != runs.end(); ++run_it) {
						if (run_it->y < y) {
							continue;
						}

						if (run_it->y > y) {
							break;
						}

						if (run_it->x_begin <= x_begin && run_it->x_end > x_begin ||
							run_it->x_begin >= x_begin && run_it->x_end <= x_end ||
							run_it->x_begin < x_end && run_it->x_end >= x_end) {

							uint16_t x_begin_cp = std::max(run_it->x_begin, x_begin);
							uint16_t x_end_cp = std::min(run_it->x_end, x_end);
							uint16_t amount = (x_end_cp - x_begin_cp);

							for (x = 0; x < amount; ++x) {
								auto off = x_begin_cp - x_begin + x;
								set_pixel_fn(src_pixels + off, dst_pixels + off);
							}
						}
					}

					src_pixels += src_w;
					dst_pixels += dst_w;
				}
			} else {
				for (y = 0; y < src_rect.height; ++y) {
					for (x = 0; x < src_rect.width; ++x) {
						if ((*src_pixels & amask) != 0) {
							set_pixel_fn(src_pixels, dst_pixels);
						}

						++src_pixels;
						++dst_pixels;
					}

					src_pixels += src_advance;
					dst_pixels += dst_advance;
				}
			}
		}
	} else {
		auto& runs = src.GetRuns();

		if (!runs.empty()) {
			auto run_it = runs.begin();

			uint16_t x_begin = src_rect.x;
			uint16_t x_end = src_rect.x + src_rect.width;

			for (int y = src_rect.y; y < src_rect.y + src_rect.height; ++y) {
				for (; run_it != runs.end(); ++run_it) {
					if (run_it->y < y) {
						continue;
					}

					if (run_it->y > y) {
						break;
					}

					if (run_it->x_begin <= x_begin && run_it->x_end > x_begin ||
						run_it->x_begin >= x_begin && run_it->x_end <= x_end ||
						run_it->x_begin < x_end && run_it->x_end >= x_end) {

						uint16_t x_begin_cp = std::max(run_it->x_begin, x_begin);
						uint16_t x_end_cp = std::min(run_it->x_end, x_end);
						uint16_t amount = (x_end_cp - x_begin_cp) * bpp;

						memcpy(
							dst_pixels + x_begin_cp - x_begin,
							src_pixels + x_begin_cp - x_begin,
							amount);
					}
				}

				src_pixels += src_w;
				dst_pixels += dst_w;
			}
		} else {
			for (int y = 0; y < src_rect.height; ++y) {
				for (int x = 0; x < src_rect.width; ++x) {
					if ((*src_pixels & amask) != 0) {
						*(pixel_type*)(dst_pixels) = *src_pixels;
					}

					++src_pixels;
					++dst_pixels;
				}

				src_pixels += src_advance;
				dst_pixels += dst_advance;
			}
		}
	}

	return true;
}

} // anonymous namespace

namespace BitmapBlit {

bool Blit(Bitmap& dest, int x, int y, Bitmap const& src, Rect src_rect,
		Opacity const& opacity, pixman_op_t blend_mode) {

	if (blend_mode == PIXMAN_OP_SRC) {
		return BlitFast(dest, x, y, src, src_rect, opacity);
	}

	if (opacity.IsTransparent()) {
		return true;
	}

	if (blend_mode != PIXMAN_OP_OVER) {
		// Not supported
		return false;
	}

	if (opacity.IsSplit()) {
		// Only used by events on bushes, not implemented here
		return false;
	}

	Rect dst_rect = {x, y, 0, 0};

	if (!AdjustRects(dest, dst_rect, src, src_rect, opacity)) {
		return true;
	}

	// This dispatching through a template function with a known pixel format is faster because
	// bits, shift etc. are known at compile time and replaced with constants by the compiler.
	if (format_A1R5G5B5_n().MatchIgnoreAlpha(src.format)) {
		return BlitT<format_A1R5G5B5_a>(dest, dst_rect, src, src_rect, opacity);
	} else if (format_A1B5G5R5_n().MatchIgnoreAlpha(src.format)) {
		return BlitT<format_A1R5G5B5_a>(dest, dst_rect, src, src_rect, opacity);
	}
	/*
	// 32bit versions for testing (not useful in production because pixman SIMD is faster)
	else if (format_R8G8B8A8_n().MatchIgnoreAlpha(src.format)) {
		return BlitT<format_R8G8B8A8_n>(dest, dst_rect, src, src_rect, opacity);
	} else if (format_B8G8R8A8_n().MatchIgnoreAlpha(src.format)) {
	return BlitT<format_B8G8R8A8_n>(dest, dst_rect, src, src_rect, opacity);
	} else if (format_A8R8G8B8_n().MatchIgnoreAlpha(src.format)) {
		return BlitT<format_A8R8G8B8_n>(dest, dst_rect, src, src_rect, opacity);
	} else if (format_A8B8G8R8_n().MatchIgnoreAlpha(src.format)) {
		return BlitT<format_A8B8G8R8_n>(dest, dst_rect, src, src_rect, opacity);
	}
	}*/

	return false;
}

bool BlitFast(Bitmap& dest, int x, int y, Bitmap const& src, Rect src_rect,
		Opacity const& opacity) {

	if (opacity.IsTransparent()) {
		return true;
	}

	if (!src.format.rgb_equal(dest.format)) {
		return false;
	}

	// Performance is exactly the same as pixman
	Rect dst_rect = {x, y, 0, 0};

	if (!AdjustRects(dest, dst_rect, src, src_rect, opacity)) {
		return true;
	}

	int bpp = src.format.bytes;
	int src_pitch = src.pitch();
	int dst_pitch = dest.pitch();

	uint8_t* src_pixels = (uint8_t*)src.pixels() + src_rect.x * bpp + src_rect.y * src_pitch;
	uint8_t* dst_pixels = (uint8_t*)dest.pixels() + dst_rect.x * bpp + dst_rect.y * dst_pitch;

	int bytes_per_row = src_rect.width * bpp;

	for (y = 0; y < src_rect.height; ++y) {
		memcpy(
			dst_pixels,
			src_pixels,
			bytes_per_row);

		src_pixels += src_pitch;
		dst_pixels += dst_pitch;
	}

	return true;
}

void ClearRect(Bitmap& dest, Rect src_rect) {
	src_rect.Adjust(dest.GetRect());
	Rect& dst_rect = src_rect;

	int bpp = dest.format.bytes;
	int dst_pitch = dest.pitch();

	uint8_t* dst_pixels = (uint8_t*)dest.pixels() + dst_rect.x * bpp + dst_rect.y * dst_pitch;

	int line_width = src_rect.width * bpp;

	for (int y = 0; y < src_rect.height; ++y) {
		memset(dst_pixels, '\0', line_width);
		dst_pixels += dst_pitch;
	}
}

} // namespace BitmapBlit
