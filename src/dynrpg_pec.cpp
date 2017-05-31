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

#include "dynrpg_pec.h"
#include "bitmap.h"
#include "game_party.h"

struct PecAttribute;

namespace {
	std::map<int, PecAttribute> attrib;
}

struct PecAttribute {
	int frame_width = 0;
	int frame_height = 0;
	int current_frame = 0;
};

void set_frame(Sprite* sprite, const PecAttribute& attrib, int frame) {

}

bool CreateSpritesheet(dyn_arg_list args) {
	DYNRPG_FUNCTION("pec_create_sprite_sheet")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, width)
	DYNRPG_GET_INT_ARG(2, height)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	PecAttribute a;
	a.frame_width = width;
	a.frame_height = height;

	attrib[picture_id] = a;

	return true;
}

bool NextFrame(dyn_arg_list args) {
	DYNRPG_FUNCTION("pec_next_frame")

	DYNRPG_CHECK_ARG_LENGTH(1);

	DYNRPG_GET_INT_ARG(0, picture_id)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	// sprite->SetSrcRect(sprite->GetBitmap()->GetRect());

	return true;
}

bool JumpToFrame(dyn_arg_list args) {
	DYNRPG_FUNCTION("pec_jump_to_frame")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, column)
	DYNRPG_GET_INT_ARG(2, row)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	//sprite->SetSrcRect(sprite->GetBitmap()->GetRect());

	return true;
}

bool DestroySpritesheet(dyn_arg_list args) {
	DYNRPG_FUNCTION("pec_destroy_sprite_sheet")

	DYNRPG_CHECK_ARG_LENGTH(1);

	DYNRPG_GET_INT_ARG(0, picture_id)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	sprite->SetSrcRect(sprite->GetBitmap()->GetRect());

	// delete from map

	return true;
}

void DynRpg::Pec::RegisterFunctions() {
	/*DynRpg::RegisterFunction("pec_set_color", SetColor);
	DynRpg::RegisterFunction("pec_get_color", GetColor);
	DynRpg::RegisterFunction("pec_set_position", SetPosition);
	DynRpg::RegisterFunction("pec_get_position", GetPosition);
	DynRpg::RegisterFunction("pec_set_transparency", SetTransparency);
	DynRpg::RegisterFunction("pec_get_transparency", GetTransparency);
	DynRpg::RegisterFunction("pec_set_magnification", SetMagnification);
	DynRpg::RegisterFunction("pec_get_magnification", GetMagnification);
	DynRpg::RegisterFunction("pec_set_rotation_angle", SetRotationAngle);
	DynRpg::RegisterFunction("pec_get_rotation_angle", GetRotationAngle);
	DynRpg::RegisterFunction("pec_is_flipped", IsFlipped);
	DynRpg::RegisterFunction("pec_flip_picture", FlipPicture);*/
	DynRpg::RegisterFunction("pec_create_sprite_sheet", CreateSpritesheet);
	DynRpg::RegisterFunction("pec_next_frame", NextFrame);
	DynRpg::RegisterFunction("pec_jump_to_frame", JumpToFrame);
	DynRpg::RegisterFunction("pec_destroy_sprite_sheet", DestroySpritesheet);
}

void DynRpg::Pec::Update() {
	// no-op
}

DynRpg::Pec::~Pec() {
	attrib.clear();
}
