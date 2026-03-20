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
#include "sprite_video.h"
#include "bitmap.h"
#include "main_data.h"
#include "game_screen.h"
#include "player.h"

Sprite_Video::Sprite_Video() {
	SetZ(Priority_Movie);
}

void Sprite_Video::Draw(Bitmap& dst) {
	const auto* screen = Main_Data::game_screen.get();

	const auto* movie = Main_Data::game_screen->GetMovie();
	if (!movie) {
		return;
	}

	auto frame = movie->GetVideoFrame();
	if (!frame) {
		return;
	}

	auto rect = screen->GetMovieRect();

	if (rect.width <= 0 || rect.height <= 0 ||
		frame->GetWidth() <= 0 || frame->GetHeight() <= 0) {
		return;
	}

	SetX(Player::menu_offset_x + rect.x);
	SetY(Player::menu_offset_y + rect.y);
	SetZoomX(static_cast<double>(rect.width) / frame->GetWidth());
	SetZoomY(static_cast<double>(rect.height) / frame->GetHeight());

	SetBitmap(frame);

	Sprite::Draw(dst);
}
