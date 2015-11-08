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
#include "output.h"

AudioInterface& DummyUi::GetAudio() {
	return *audio_;
}

DummyUi::DummyUi(int width, int height) :
	BaseUi() {
	audio_.reset(new EmptyAudio());
	const DynamicFormat format(
		32,
		0xFF000000,
		0x00FF0000,
		0x0000FF00,
		0x000000FF,
		PF::NoAlpha);
	Bitmap::SetFormat(format);
	main_surface = Bitmap::Create(width, height, false, 32);
}

DummyUi::~DummyUi() {
}

uint32_t DummyUi::GetTicks() const {
	return 0;
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
	// no-op
}

void DummyUi::UpdateDisplay() {
}

void DummyUi::BeginScreenCapture() {
	//CleanDisplay();
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
