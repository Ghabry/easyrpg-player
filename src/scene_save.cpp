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
#include <sstream>

#ifdef EMSCRIPTEN
#  include <emscripten.h>
#endif

#include "data.h"
#include "filefinder.h"
#include "game_actor.h"
#include "game_map.h"
#include "game_party.h"
#include "lsd_reader.h"
#include "output.h"
#include "player.h"
#include "scene_save.h"
#include "scene_file.h"
#include "reader_util.h"

Scene_Save::Scene_Save() :
	Scene_File(Data::terms.save_game_message) {
	Scene::type = Scene::Save;
}

void Scene_Save::Start() {
	Scene_File::Start();

	for (int i = 0; i < 15; i++) {
		file_windows[i]->SetHasSave(true);
		file_windows[i]->Refresh();
	}
}

void Scene_Save::Action(int index) {
	std::stringstream ss;
	ss << "Save" << (index <= 8 ? "0" : "") << (index + 1) << ".lsd";

	Output::Debug("Saving to %s", ss.str().c_str());

	std::string save_file = ss.str();
	std::string save_name = FileFinder::FindDefault(*tree, ss.str());

	if (save_name.empty()) {
		save_name = FileFinder::MakePath((*tree).directory_path, save_file);
	}

	Player::WriteSavegame(save_name, index + 1);

	Scene::Pop();
}

bool Scene_Save::IsSlotValid(int) {
	return true;
}
