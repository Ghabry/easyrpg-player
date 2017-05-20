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
#include "game_switches.h"
#include "game_variables.h"

struct PecAttribute;

namespace {
	std::map<int, PecAttribute> attrib;
}

struct PecAttribute {
	int frame_rows = 0;
	int frame_cols = 0;
	int frame_width = 0;
	int frame_height = 0;
	int current_frame = 0;
};

static bool SetColor(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_set_color")

	DYNRPG_CHECK_ARG_LENGTH(5);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, red)
	DYNRPG_GET_INT_ARG(2, green)
	DYNRPG_GET_INT_ARG(3, blue)
	DYNRPG_GET_INT_ARG(4, saturation)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	//picture->SetColorEffect(red, green, blue, saturation);
	//picture->SetTransition(0);

	return true;
}

static bool GetColor(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_get_color")

	DYNRPG_CHECK_ARG_LENGTH(5);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_red)
	DYNRPG_GET_INT_ARG(2, out_green)
	DYNRPG_GET_INT_ARG(3, out_blue)
	DYNRPG_GET_INT_ARG(4, out_saturation)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	Tone tone = sprite->GetTone();

	if (out_red > 0) {
		Game_Variables[out_red] = (int)(255.0 / tone.red * 100);
	}
	if (out_green > 0) {
		Game_Variables[out_red] = (int)(255.0 / tone.green * 100);
	}
	if (out_blue > 0) {
		Game_Variables[out_blue] = (int)(255.0 / tone.blue * 100);
	}
	if (out_saturation > 0) {
		Game_Variables[out_saturation] = (int)(255.0 / tone.gray * 100);
	}

	return true;
}

static bool SetPosition(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_set_position")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, x)
	DYNRPG_GET_INT_ARG(2, y)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	//icture->SetMovementEffect(x, y);
	//picture->SetTransition(0);

	return true;
}

static bool GetPosition(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_get_position")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_x)
	DYNRPG_GET_INT_ARG(2, out_y)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	if (out_x > 0) {
		Game_Variables[out_x] = sprite->GetX();
	}
	if (out_y > 0) {
		Game_Variables[out_y] = sprite->GetY();
	}

	return true;
}

static bool SetTransparency(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_set_transparency")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, transparency)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	//picture->SetTransparencyEffect(transparency, transparency);
	//picture->SetTransition(0);

	return true;
}

static bool GetTransparency(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_get_transparency")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_transparency)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	if (out_transparency > 0) {
		int o = (int)(100.0 * sprite->GetOpacity(0) / 255.0);
		Game_Variables[out_transparency] = o;
	}

	return true;
}

static bool SetZoom(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_set_magnification")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, zoom)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	//picture->SetZoomEffect(zoom);
	//picture->SetTransition(0);

	return true;
}

static bool GetZoom(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_get_magnification")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_zoom)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	if (out_zoom > 0) {
		int o = (int)(sprite->GetZoomX() * 100.0);
		Game_Variables[out_zoom] = o;
	}

	return true;
}

/*
static bool SetRotation(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_set_rotation_angle")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, angle)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	// Not supported by our API

	return true;
}
*/

static bool GetRotation(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_get_rotation_angle")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_angle)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	if (out_angle > 0) {
		int a = (int)(sprite->GetAngle() / 256.0 * 360.0);
		Game_Variables[out_angle] = a;
	}

	return true;
}

static bool SetEffectStrength(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_set_effect_strength")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, strength)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	///picture->SetWaverEffect(strength);
	//picture->SetTransition(0);

	return true;
}

static bool GetEffectStrength(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_get_effect_strength")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_strength)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	if (out_strength > 0) {
		Game_Variables[out_strength] = sprite->GetWaverDepth();
	}

	return true;
}

static bool FlipHorizontal(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_flip_picture")

	DYNRPG_CHECK_ARG_LENGTH(1);

	DYNRPG_GET_INT_ARG(0, picture_id)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	sprite->SetFlipX(true);

	return true;
}

static bool IsFlippedHorizontal(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_is_flipped")

	DYNRPG_CHECK_ARG_LENGTH(2);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, out_flipped) // Switch

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	const Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	if (out_flipped > 0) {
		Game_Variables[out_flipped] = sprite->GetFlipX();
	}

	return true;
}

void set_frame(Sprite* sprite, const PecAttribute& attrib) {
	int sx = 0;
	int sy = 0;
	if (attrib.current_frame >= 0) {
		sx = attrib.current_frame % attrib.frame_cols * attrib.frame_width;
		sy = attrib.current_frame / attrib.frame_cols * attrib.frame_height;
	}
	Rect r;
	r.Set(sx, sy, attrib.frame_width, attrib.frame_height);
	sprite->SetSrcRect(r);
}

static bool CreateSpritesheet(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_create_sprite_sheet")

	DYNRPG_CHECK_ARG_LENGTH(3);

	DYNRPG_GET_INT_ARG(0, picture_id)
	DYNRPG_GET_INT_ARG(1, width)
	DYNRPG_GET_INT_ARG(2, height)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}
	if (!sprite->GetBitmap()) {
		return true;
	}

	PecAttribute a;
	a.frame_width = width;
	a.frame_height = height;
	a.frame_cols = sprite->GetBitmap()->GetWidth() / width;
	a.frame_rows = sprite->GetBitmap()->GetHeight() / height;
	attrib[picture_id] = a;

	set_frame(sprite, a);

	return true;
}

static bool NextFrame(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_next_frame")

	DYNRPG_CHECK_ARG_LENGTH(1);

	DYNRPG_GET_INT_ARG(0, picture_id)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	auto it = attrib.find(picture_id);

	if (it == attrib.end()) {
		return true;
	}

	PecAttribute& a = (*it).second;
	a.current_frame++;
	set_frame(sprite, a);

	return true;
}

static bool JumpToFrame(const dyn_arg_list& args) {
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

	auto it = attrib.find(picture_id);

	if (it == attrib.end()) {
		return true;
	}

	PecAttribute& a = (*it).second;
	a.current_frame = column + row * a.frame_cols;
	set_frame(sprite, a);

	return true;
}

static bool DestroySpritesheet(const dyn_arg_list& args) {
	DYNRPG_FUNCTION("pec_destroy_sprite_sheet")

	DYNRPG_CHECK_ARG_LENGTH(1);

	DYNRPG_GET_INT_ARG(0, picture_id)

	Game_Picture* picture = Main_Data::game_screen->GetPicture(picture_id);
	Sprite* sprite = picture->GetSprite();

	if (!sprite) {
		return true;
	}

	attrib.erase(picture_id);

	return true;
}

std::string DynRpg::Pec::GetIdentifier() {
	return "DynPEC";
}

void DynRpg::Pec::RegisterFunctions() {
	DynRpg::RegisterFunction("pec_set_color", SetColor);
	DynRpg::RegisterFunction("pec_get_color", GetColor);
	DynRpg::RegisterFunction("pec_set_position", SetPosition);
	DynRpg::RegisterFunction("pec_get_position", GetPosition);
	DynRpg::RegisterFunction("pec_set_transparency", SetTransparency);
	DynRpg::RegisterFunction("pec_get_transparency", GetTransparency);
	DynRpg::RegisterFunction("pec_set_magnification", SetZoom);
	DynRpg::RegisterFunction("pec_get_magnification", GetZoom);
	//DynRpg::RegisterFunction("pec_set_rotation_angle", SetRotation);
	DynRpg::RegisterFunction("pec_get_rotation_angle", GetRotation);
	DynRpg::RegisterFunction("pec_set_effect_strength", SetEffectStrength);
	DynRpg::RegisterFunction("pec_get_effect_strength", GetEffectStrength);
	DynRpg::RegisterFunction("pec_is_flipped", IsFlippedHorizontal);
	DynRpg::RegisterFunction("pec_flip_picture", FlipHorizontal);
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
