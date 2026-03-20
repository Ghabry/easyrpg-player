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

#ifndef EP_SPRITE_VIDEO_H
#define EP_SPRITE_VIDEO_H

// Headers
#include "sprite.h"

/**
 * Sprite for a video playback.
 *
 * Draw will always render the frame at the current playback position.
 * The playback of the file itself is handled by the Audio Decoder Thread.
 */
class Sprite_Video : public Sprite {
public:
	Sprite_Video();
	void Draw(Bitmap& dst) override;

private:

};

#endif
