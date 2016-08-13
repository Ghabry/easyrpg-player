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
#include <iostream>
#include <sstream>
#include "audio.h"
#include "bitmap.h"
#include "dummy_ui.h"
#include "player.h"
#include "system.h"
#include "graphics.h"
#include "output.h"
#include "scene.h"
#include "game_party.h"
#include "game_actors.h"
#include "game_actor.h"
#include "game_message.h"

DummyUi::DummyUi(int width, int height) :
	BaseUi() {
	const DynamicFormat format(
		32,
		0xFF000000,
		0x00FF0000,
		0x0000FF00,
		0x000000FF,
		PF::Alpha);
	Bitmap::SetFormat(format);

	current_display_mode.width = width;
	current_display_mode.height = height;
	current_display_mode.bpp = 32;

	main_surface = Bitmap::Create(width, height, false, 32);
}

DummyUi::~DummyUi() {
}

uint32_t DummyUi::GetTicks() const {
	return ticks;
}

void DummyUi::Sleep(uint32_t) {
	//no-op
}

void DummyUi::BeginDisplayModeChange() {
	// no-op
}

void DummyUi::EndDisplayModeChange() {
	// no-op
}

void DummyUi::Resize(long /*width*/, long /*height*/) {
	// no-op
}

void DummyUi::ToggleFullscreen() {
	// no-op
}

void DummyUi::ToggleZoom() {
	// no-op
}

bool DummyUi::IsFullscreen() {
	return true;
}

void DummyUi::ProcessEvents() {
	++ticks;

	if (!Graphics::IsTransitionPending()) {
		if (want_screenshot && !draw_now) {
			Output::TakeScreenshot();

			want_screenshot = false;
		}
	}

	if (Player::exit_flag) {
		return;
	}

	if (Scene::instance->type == Scene::GameBrowser || Scene::instance->type == Scene::Gameover) {
		Player::exit_flag = true;
	} else if (Scene::instance->type == Scene::Title || Scene::instance->type == Scene::Logo) {
		if (Graphics::IsTransitionPending()) {
			return;
		}

		if (skipped_title) {
			// returned to title
			Player::exit_flag = true;
		}

		title_screenshot++;

		if (title_screenshot == 5) {
			want_screenshot = true;
			draw_now = true;
		}

		if (title_screenshot > 10) {
			keys[Input::Keys::RETURN] = !keys[Input::Keys::RETURN];
		}
	} else if (Scene::instance->type == Scene::Map || Scene::instance->type == Scene::Battle) {
		for (const auto& actor : Main_Data::game_party->GetActors()) {
			actor->ChangeHp(actor->GetMaxHp());
			actor->SetSp(actor->GetMaxSp());
			actor->RemoveAllStates();
		}

		if (Scene::instance->type == Scene::Battle || Game_Message::visible) {
			int rand = Utils::GetRandomNumber(0, 5);

			keys[Input::Keys::RETURN] = rand == 0;
			keys[Input::Keys::ESCAPE] = rand == 1;
			keys[Input::Keys::LEFT] = rand == 2;
			keys[Input::Keys::RIGHT] = rand == 3;
			keys[Input::Keys::UP] = rand == 4;
			keys[Input::Keys::DOWN] = rand == 5;
		} else {
			int rand = Utils::GetRandomNumber(0, 50);

			keys[Input::Keys::RETURN] = rand == 0;
			//keys[Input::Keys::ESCAPE] = rand == 1;
			keys[Input::Keys::LEFT] = rand >= 2 && rand <= 2 + 5;
			keys[Input::Keys::RIGHT] = rand >  2 + 5 && rand <=  2 + 5 * 2;
			keys[Input::Keys::UP] = rand > 2 + 5 * 2 && rand <= 2 + 5 * 3;
			keys[Input::Keys::DOWN] = rand > 2 + 5 * 3 && rand <= 2 + 5 * 4;
		}
	} else {
		Output::Debug("SCENE %s: ", Scene::scene_names[Scene::instance->type]);
		Player::exit_flag = true;
	}

	if (ticks == 500 || ticks % 1800 == 0) {
		want_screenshot = true;
		draw_now = true;
	}

	if (ticks == 1800 * 7 + 1) {
		Player::exit_flag = true;
	}
}

void DummyUi::UpdateDisplay() {
}

void DummyUi::BeginScreenCapture() {
	CleanDisplay();
}

BitmapRef DummyUi::EndScreenCapture() {
	return main_surface;
}

void DummyUi::SetTitle(const std::string& /* title */) {
	// no-op
}

bool DummyUi::ShowCursor(bool /* flag */) {
	return true;
}
