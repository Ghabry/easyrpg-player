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

#ifndef EP_BITMAP_BLIT_H
#define EP_BITMAP_BLIT_H

#include "bitmap.h"
#include <pixman.h>

namespace BitmapBlit {
	bool Blit(Bitmap& dest, int x, int y, Bitmap const& src, Rect src_rect,
		Opacity const& opacity, pixman_op_t blend_mode);

	bool BlitFast(Bitmap& dest, int x, int y, Bitmap const& src, Rect src_rect,
		Opacity const& opacity);

	void ClearRect(Bitmap& dest, Rect src_rect);
}

#endif
