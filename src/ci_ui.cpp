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
#include "ci_ui.h"
#include "bitmap.h"
#include "pixel_format.h"

CiUi::CiUi(long width, long height, const Game_Config& cfg) : BaseUi(cfg) {
	current_display_mode.width = width;
	current_display_mode.height = height;
	current_display_mode.bpp = 32;

	const auto format = format_B8G8R8A8_n().format();
	Bitmap::SetFormat(Bitmap::ChooseFormat(format));

	main_surface = Bitmap::Create(
		current_display_mode.width,
		current_display_mode.height,
		false,
		current_display_mode.bpp
	);

#ifdef SUPPORT_AUDIO
	audio_ = std::make_unique<EmptyAudio>(cfg.audio);
#endif
}

bool CiUi::ProcessEvents() {
	return true;
}

void CiUi::UpdateDisplay() {
}

void CiUi::vGetConfig(Game_ConfigVideo& cfg) const {
	cfg = vcfg;
}

#ifdef SUPPORT_AUDIO
AudioInterface& CiUi::GetAudio() {
	return *audio_;
}
#endif
