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
#include "player.h"
#include "output.h"
#include "game_system.h"
#include "scene_save.h"

#include <fmt/format.h>

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

void CiUi::ProcessCi() {
	if (!config.ci_flag) {
		return;
	}

	auto fs = FileFinder::Save();

	if (!config.ci_name.empty()) {
		fs = FileFinder::Root().Subtree(config.ci_name);
	}

	if (!fs) {
		Output::Error("Directory {} does not exist", config.ci_name);
	}

	if (!Main_Data::game_system) {
		return;
	}

	auto frame = Main_Data::game_system->GetFrameCounter();

	if (Scene::Find(Scene::Map) && frame > 0) {
		if (config.ci_save) {
			Scene_Save::Save(fs, frame, false, true);
		}

		if (config.ci_screenshot) {
			auto os = fs.OpenOutputStream(fmt::format("frame{:02}.png", frame));
			Output::TakeScreenshot(os);
		}
	}

	if (config.ci_exit > 0 && frame >= config.ci_exit) {
		Player::exit_flag = true;
	}
}
