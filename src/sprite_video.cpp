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
#include "player.h"
#include "video_decoder.h"

Sprite_Video::Sprite_Video() {
	SetZ(Priority_Movie);
}

void Sprite_Video::Draw(Bitmap& dst) {
	if (!video) {
		return;
	}

	auto frame = video->GetCurrentVideoFrame();
	if (!frame) {
		return;
	}

	if (video_rect.width <= 0 || video_rect.height <= 0 ||
		frame->GetWidth() <= 0 || frame->GetHeight() <= 0) {
		return;
	}

	if (Player::game_config.fake_resolution.Get()) {
		SetX(Player::menu_offset_x + video_rect.x);
		SetY(Player::menu_offset_y + video_rect.y);
	} else {
		SetX(video_rect.x);
		SetY(video_rect.y);
	}

	SetZoomX(static_cast<double>(video_rect.width) / frame->GetWidth());
	SetZoomY(static_cast<double>(video_rect.height) / frame->GetHeight());

	SetBitmap(frame);

	Sprite::Draw(dst);
}

VideoDecoder* Sprite_Video::GetVideo() const {
	return video;
}

void Sprite_Video::SetVideo(VideoDecoder* video) {
	this->video = video;
}

Rect Sprite_Video::GetVideoRect() const {
	return video_rect;
}

void Sprite_Video::SetVideoRect(const Rect& video_rect) {
	this->video_rect = video_rect;
}
