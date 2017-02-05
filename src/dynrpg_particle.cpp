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
#include <map>

#include "dynrpg_particle.h"
#include "baseui.h"
#include "bitmap.h"
#include "game_actors.h"
#include "game_map.h"
#include "game_party.h"
#include "game_screen.h"
#include "game_variables.h"
#include "graphics.h"
#include "main_data.h"
#include "player.h"
#include "sprite.h"

static bool WriteText(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("write_text")

	DYNRPG_CHECK_ARG_LENGTH(4);

	DYNRPG_GET_STR_ARG(0, id)
	DYNRPG_GET_INT_ARG(1, x)
	DYNRPG_GET_INT_ARG(2, y)
	DYNRPG_GET_STR_ARG(3, text)

	/*std::string new_id = DynRpgText::Substitute(id);
	graphics[new_id] = std::unique_ptr<DynRpgText>(new DynRpgText(1, x, y + 2, text));

	if (args.size() > 4) {
		DYNRPG_GET_STR_ARG(4, fixed)
		graphics[id]->SetFixed(fixed == "fixed");
	}

	if (args.size() > 5) {
		DYNRPG_GET_INT_ARG(5, color)
		graphics[id]->SetColor(color);
	}

	if (args.size() > 6) {
		DYNRPG_GET_INT_ARG(6, pic_id)
		graphics[id]->SetPictureId(pic_id);
	}*/

	return true;
}

std::string DynRpg::Particle::GetIdentifier() {
	return "KazeParticles";
}

void DynRpg::Particle::RegisterFunctions() {
/*	DynRpg::RegisterFunction("write_text", WriteText);
	DynRpg::RegisterFunction("append_line", AppendLine);
	DynRpg::RegisterFunction("append_text", AppendText);
	DynRpg::RegisterFunction("change_text", ChangeText);
	DynRpg::RegisterFunction("change_position", ChangePosition);
	DynRpg::RegisterFunction("remove_text", RemoveText);
	DynRpg::RegisterFunction("remove_all", RemoveAll);*/
}

void DynRpg::Particle::Update() {
	/*for (auto& g : graphics) {
		g.second->Update();
	}*/
}

DynRpg::Particle::~Particle() {

}
