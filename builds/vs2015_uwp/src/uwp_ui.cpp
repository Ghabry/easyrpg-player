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
#include "pch.h"
#include "uwp_ui.h"
#include "color.h"
#include "graphics.h"
#include "keys.h"
#include "output.h"
#include "player.h"
#include "bitmap.h"
#include <iostream>
#include <chrono>

#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>

#include <cstring>
#include <stdio.h>

#ifdef SUPPORT_AUDIO
#include "audio_3ds.h"
AudioInterface& UwpUi::GetAudio() {
	return *audio_;
}
#endif

UwpUi::UwpUi(int width, int height) :
	BaseUi() {
	
	current_display_mode.width = width;
	current_display_mode.height = height;
	current_display_mode.bpp = 32;
	const DynamicFormat format(
		32,
		0x000000FF,
		0x0000FF00,
		0x00FF0000,
		0xFF000000,
		PF::NoAlpha);
	Bitmap::SetFormat(Bitmap::ChooseFormat(format));
	main_surface = Bitmap::Create(width, height, true, 32);

	#ifdef SUPPORT_AUDIO
		audio_.reset(new CtrAudio());
	#endif	
}

UwpUi::~UwpUi() {

}

void UwpUi::Sleep(uint32_t time) {
	::Sleep(time);
}

uint32_t UwpUi::GetTicks() const {
	auto now = std::chrono::system_clock::now();
	auto duration = now.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

	return static_cast<uint32_t>(millis);
}
void UwpUi::BeginDisplayModeChange() {
	// no-op
}

void UwpUi::EndDisplayModeChange() {
	// no-op
}

void UwpUi::Resize(long /*width*/, long /*height*/) {
	// no-op
}

void UwpUi::ToggleFullscreen() {
	// no-op
}

void UwpUi::ToggleZoom() {
	// no-op
}

bool UwpUi::IsFullscreen() {
	return true;
}

void UwpUi::ProcessEvents() {
	/*
	hidScanInput();
	u32 input = hidKeysHeld();
	keys[Input::Keys::Z] = (input & KEY_A);
	keys[Input::Keys::X] = (input & KEY_B);
	keys[Input::Keys::N8] = (input & KEY_X);
	keys[Input::Keys::LSHIFT] = (input & KEY_Y);
	keys[Input::Keys::F12] = (input & KEY_SELECT);
	keys[Input::Keys::ESCAPE] = (input & KEY_START);
	keys[Input::Keys::RIGHT] = (input & KEY_DRIGHT);
	keys[Input::Keys::LEFT] = (input & KEY_DLEFT);
	keys[Input::Keys::UP] = (input & KEY_DUP);
	keys[Input::Keys::DOWN] = (input & KEY_DDOWN);
	keys[Input::Keys::F2] = (input & KEY_L);
	
	//Fullscreen mode support
	bool old_state = trigger_state;
	trigger_state = (input & KEY_R);
	if ((trigger_state != old_state) && trigger_state) fullscreen = !fullscreen;
	
	//CirclePad support
	circlePosition circlepad;
	hidCircleRead(&circlepad);
	
	if (circlepad.dy > 25) keys[Input::Keys::UP] = true;
	else if (circlepad.dy < -25) keys[Input::Keys::DOWN] = true;
	else if (circlepad.dx > 25) keys[Input::Keys::RIGHT] = true;
	else if (circlepad.dx < -25) keys[Input::Keys::LEFT] = true;
	*/	
}

void UwpUi::UpdateDisplay() {
	
}

void UwpUi::BeginScreenCapture() {
	CleanDisplay();
}

BitmapRef UwpUi::EndScreenCapture() {
	return Bitmap::Create(*main_surface, main_surface->GetRect());
}

void UwpUi::SetTitle(const std::string& /* title */) {
	// no-op
}

bool UwpUi::ShowCursor(bool /* flag */) {
	return true;
}
