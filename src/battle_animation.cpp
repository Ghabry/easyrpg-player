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

#include "bitmap.h"
#include "async_handler.h"
#include "rpg_animation.h"
#include "output.h"
#include "game_battle.h"
#include "game_system.h"
#include "game_map.h"
#include "graphics.h"
#include "main_data.h"
#include "filefinder.h"
#include "cache.h"
#include "battle_animation.h"
#include "baseui.h"
#include "spriteset_battle.h"
#include "metrics.h"

BattleAnimation::BattleAnimation(const RPG::Animation& anim) :
	animation(anim), frame(0), z(1500), frame_update(false), large(false)
{
	const std::string& name = animation.animation_name;
	BitmapRef graphic;

	if (name.empty()) return;

	// Emscripten handled special here because of the FileFinder checks
	// Filefinder is not reliable for Emscripten because the files must be
	// downloaded first.
	// And we can't rely on "success" state of FileRequest because it's always
	// true on desktop.
#ifdef EMSCRIPTEN
	FileRequestAsync* request = AsyncHandler::RequestFile("Battle", animation.animation_name);
	request->Bind(&BattleAnimation::OnBattleSpriteReady, this);
	request->Start();
#else
	if (!FileFinder::FindImage("Battle", name).empty()) {
		FileRequestAsync* request = AsyncHandler::RequestFile("Battle", animation.animation_name);
		request->Bind(&BattleAnimation::OnBattleSpriteReady, this);
		request->Start();
	}
	else if (!FileFinder::FindImage("Battle2", name).empty()) {
		FileRequestAsync* request = AsyncHandler::RequestFile("Battle2", animation.animation_name);
		request->Bind(&BattleAnimation::OnBattle2SpriteReady, this);
		request->Start();
	}
	else {
		Output::Warning("Couldn't find animation: %s", name.c_str());
	}
#endif
}

int BattleAnimation::GetZ() const {
	return z;
}

void BattleAnimation::SetZ(int nz) {
	z = nz;
}

DrawableType BattleAnimation::GetType() const {
	return TypeDefault;
}

void BattleAnimation::Update() {
	if (frame_update) {
		frame++;
		RunTimedSfx();
	}
	frame_update = !frame_update;
}

void BattleAnimation::SetFrame(int _frame) {
	frame = _frame;
}

int BattleAnimation::GetFrame() const {
	return frame;
}

int BattleAnimation::GetFrames() const {
	return animation.frames.size();
}

bool BattleAnimation::IsDone() const {
	return GetFrame() >= GetFrames();
}

void BattleAnimation::OnBattleSpriteReady(FileRequestResult* result) {
	if (result->success) {
		screen = Cache::Battle(result->file);
	}
	else {
		// Try battle2
		FileRequestAsync* request = AsyncHandler::RequestFile("Battle2", result->file);
		request->Bind(&BattleAnimation::OnBattle2SpriteReady, this);
		request->Start();
	}
}

void BattleAnimation::OnBattle2SpriteReady(FileRequestResult* result) {
	if (result->success) {
		screen = Cache::Battle2(result->file);
	}
	else {
		Output::Warning("Couldn't find animation: %s", result->file.c_str());
	}
}

void BattleAnimation::DrawAt(int x, int y) {
	if (!screen) return; // Initialization failed
	if (IsDone()) return;

	const RPG::AnimationFrame& anim_frame = animation.frames[frame];

	std::vector<RPG::AnimationCellData>::const_iterator it;
	for (it = anim_frame.cells.begin(); it != anim_frame.cells.end(); ++it) {
		const RPG::AnimationCellData& cell = *it;

		if (!cell.valid) {
			// Skip unused cells (they are created by deleting cells in the
			// animation editor, resulting in gaps)
			continue;
		}

		int sx = cell.cell_id % 5;
		int sy = cell.cell_id / 5;
		int size = large ? 128 : 96;
		Rect src_rect(sx * size, sy * size, size, size);
		Tone tone(cell.tone_red * 128 / 100,
			cell.tone_green * 128 / 100,
			cell.tone_blue * 128 / 100,
			cell.tone_gray * 128 / 100);
		int opacity = 255 * (100 - cell.transparency) / 100;
		double zoom = cell.zoom / 100.0;
		DisplayUi->GetDisplaySurface()->EffectsBlit(
			x + cell.x, y + cell.y,
			size / 2, size / 2,
			*screen, src_rect,
			opacity, tone,
			zoom, zoom);
	}
}

// FIXME: looks okay, but needs to be measured
static int flash_duration = 5;

void BattleAnimation::RunTimedSfx() {
	// Lookup any timed SFX (SE/flash/shake) data for this frame
	std::vector<RPG::AnimationTiming>::const_iterator it = animation.timings.begin();
	for (; it != animation.timings.end(); ++it) {
		if (it->frame == GetFrame()) {
			ProcessAnimationTiming(*it);
		}
	}
}

void BattleAnimation::ProcessAnimationTiming(const RPG::AnimationTiming& timing) {
	// Play the SE.
	Game_System::SePlay(timing.se);

	// Flash.
	if (timing.flash_scope == RPG::AnimationTiming::FlashScope_target) {
		Flash(Color(timing.flash_red << 3,
			timing.flash_green << 3,
			timing.flash_blue << 3,
			timing.flash_power << 3));
	} else if (timing.flash_scope == RPG::AnimationTiming::FlashScope_screen && ShouldScreenFlash()) {
		Main_Data::game_screen->FlashOnce(
			timing.flash_red << 3,
			timing.flash_green << 3,
			timing.flash_blue << 3,
			timing.flash_power << 3,
			flash_duration);
	}

	// TODO: Shake.
}

// For handling the vertical position.
// (The first argument should be an RPG::Animation::Position,
// but the position member is an int, so take an int.)
static int CalculateOffset(int pos, int target_height) {
	switch (pos) {
	case RPG::Animation::Position_down:
		return target_height / 2;
	case RPG::Animation::Position_up:
		return -(target_height / 2);
	default:
		return 0;
	}
}

/////////

BattleAnimationChara::BattleAnimationChara(const RPG::Animation& anim, Game_Character& chara) :
	BattleAnimation(anim), character(chara)
{
	Graphics::RegisterDrawable(this);
}
BattleAnimationChara::~BattleAnimationChara() {
	Graphics::RemoveDrawable(this);
}
void BattleAnimationChara::Draw() {
	const int character_height = 24;
	int vertical_center = character.GetScreenY() - character_height/2;
	int offset = CalculateOffset(animation.position, character_height);
	DrawAt(character.GetScreenX(), vertical_center + offset);
}
void BattleAnimationChara::Flash(Color c) {
	character.Flash(c, flash_duration);
}
bool BattleAnimationChara::ShouldScreenFlash() const { return true; }

/////////

BattleAnimationBattlers::BattleAnimationBattlers(const RPG::Animation& anim, Game_Battler& batt, bool flash) :
	BattleAnimation(anim), battlers(std::vector<Game_Battler*>(1, &batt)), should_flash(flash)
{
	Graphics::RegisterDrawable(this);
}
BattleAnimationBattlers::BattleAnimationBattlers(const RPG::Animation& anim, const std::vector<Game_Battler*>& batts, bool flash) :
	BattleAnimation(anim), battlers(batts), should_flash(flash)
{
	Graphics::RegisterDrawable(this);
}
BattleAnimationBattlers::~BattleAnimationBattlers() {
	Graphics::RemoveDrawable(this);
}
void BattleAnimationBattlers::Draw() {
	for (std::vector<Game_Battler*>::const_iterator it = battlers.begin();
	     it != battlers.end(); ++it) {
		const Game_Battler& battler = **it;
		const Sprite_Battler* sprite = Game_Battle::GetSpriteset().FindBattler(&battler);
		int offset = 0;
		if (sprite && sprite->GetBitmap()) {
			offset = CalculateOffset(animation.position, sprite->GetBitmap()->GetHeight());
		}
		DrawAt(battler.GetBattleX(), battler.GetBattleY() + offset);
	}
}
void BattleAnimationBattlers::Flash(Color c) {
	for (std::vector<Game_Battler*>::const_iterator it = battlers.begin();
	     it != battlers.end(); ++it) {
		Sprite_Battler* sprite = Game_Battle::GetSpriteset().FindBattler(*it);
		if (sprite)
			sprite->Flash(c, flash_duration);
	}
}
bool BattleAnimationBattlers::ShouldScreenFlash() const { return should_flash; }

/////////

BattleAnimationGlobal::BattleAnimationGlobal(const RPG::Animation& anim) :
	BattleAnimation(anim)
{
	Graphics::RegisterDrawable(this);
}
BattleAnimationGlobal::~BattleAnimationGlobal() {
	Graphics::RemoveDrawable(this);
}
void BattleAnimationGlobal::Draw() {
	// The animations are played at the vertices of a regular grid,
	// 20 tiles wide by 10 tiles high, independant of the map.
	// NOTE: not accurate, but see #574
	const int x_stride = 20 * Metrics::ChipSet::TileSize();
	const int y_stride = 10 * Metrics::ChipSet::TileSize();
	int x_offset = (Game_Map::GetDisplayX()/Metrics::ChipSet::TileSize()) % x_stride;
	int y_offset = (Game_Map::GetDisplayY()/Metrics::ChipSet::TileSize()) % y_stride;
	for (int y = 0; y != 3; ++y) {
		for (int x = 0; x != 3; ++x) {
			DrawAt(x_stride*x - x_offset, y_stride*y - y_offset);
		}
	}
}
void BattleAnimationGlobal::Flash(Color) {
	// nop
}
bool BattleAnimationGlobal::ShouldScreenFlash() const { return true; }
