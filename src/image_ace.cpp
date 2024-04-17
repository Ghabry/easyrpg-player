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
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include "output.h"
#include "image_ace.h"
#include "utils.h"

#define CUTE_ASEPRITE_IMPLEMENTATION
#include "external/cute_aseprite.h"

bool ImageACE::ReadACE(Filesystem_Stream::InputStream& stream, bool transparent, int& width, int& height, std::vector<Frame>& frames, std::vector<Animation>& animations) {
	(void)transparent; // TODO

	auto buf = Utils::ReadStream(stream);

	ase_t* ase = cute_aseprite_load_from_memory(buf.data(), buf.size(), nullptr);

	width = ase->w;
	height = ase->h;
	unsigned mem = width * height * 4;

	for (int i = 0; i < ase->frame_count; ++i) {
		auto& frame = ase->frames[i];
		Frame f;
		f.pixels = malloc(mem);
		memcpy(f.pixels, frame.pixels, mem);
		f.duration_ms = frame.duration_milliseconds;
		frames.push_back(f);
	}

    for (int i = 0; i < ase->tag_count; ++i) {
        auto& tag = ase->tags[i];
		Animation a;
		a.name = tag.name;
		a.frame_beg = tag.from_frame;
		a.frame_end = tag.to_frame;
		animations.push_back(a);
    }

	cute_aseprite_free(ase);

	return true;
}
