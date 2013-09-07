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
#include "input.h"
#include "player.h"
#include "system.h"
#include "baseui.h"

#include <algorithm>
#include <boost/lambda/lambda.hpp>

EASYRPG_ARRAY<std::vector<int>, Input_::BUTTON_COUNT> Input_::buttons;
EASYRPG_ARRAY<std::vector<int>, 10> Input_::dir_buttons;

bool Input_::IsWaitingInput() { return wait_input; }
void Input_::WaitInput(bool v) { wait_input = v; }

Input_::Input_()
		: start_repeat_time(20)
		, repeat_time(5)
		, wait_input(false)
{
	static bool button_initialized = false;
	if (!button_initialized) {
		button_initialized = true;
		InitButtons();
	}

	std::fill(press_time.begin(), press_time.end(), 0);
}

void Input_::Update() {
	wait_input = false; // clear each frame

	DisplayUi->ProcessEvents();
	BaseUi::KeyStatus& keystates = DisplayUi->GetKeyStates();

	// Check button states
	for (unsigned i = 0; i < BUTTON_COUNT; ++i) {
		bool pressed = false;

		// Check state of keys assigned to button
		for (unsigned e = 0; e < buttons[i].size(); e++) {
			if (keystates[buttons[i][e]]) {
				pressed = true;
				break;
			}
		}

		if (pressed) {
			released[i] = false;
			press_time[i] += 1;
		} else {
			released[i] = press_time[i] > 0;
			press_time[i] = 0;
		}

		if (press_time[i] > 0) {
			triggered[i] = press_time[i] == 1;
			repeated[i] = press_time[i] == 1 || (press_time[i] >= start_repeat_time &&	press_time[i] % repeat_time == 0);
		} else {
			triggered[i] = false;
			repeated[i] = false;
		}
	}

	// Press time for directional buttons, the less they have been pressed, the higher their priority will be
	int dirpress[10];

	// Get max pressed time for each directional button
	for (unsigned i = 1; i < 10; i++) {
		dirpress[i] = 0;
		for (unsigned e = 0; e < dir_buttons[i].size(); e++) {
			if (dirpress[i] < press_time[dir_buttons[i][e]])
				dirpress[i] = press_time[dir_buttons[i][e]];
		}
	}

	// Calculate diagonal directions pressed time by dir4 combinations
	dirpress[1] += (dirpress[2] > 0 && dirpress[4] > 0) ? dirpress[2] + dirpress[4] : 0;
	dirpress[3] += (dirpress[2] > 0 && dirpress[6] > 0) ? dirpress[2] + dirpress[6] : 0;
	dirpress[7] += (dirpress[8] > 0 && dirpress[4] > 0) ? dirpress[8] + dirpress[4] : 0;
	dirpress[9] += (dirpress[8] > 0 && dirpress[6] > 0) ? dirpress[8] + dirpress[6] : 0;

	dir4 = 0;
	dir8 = 0;

	// Check if no opposed keys are being pressed at the same time
	if (!(dirpress[2] > 0 && dirpress[8] > 0) && !(dirpress[4] > 0 && dirpress[6] > 0)) {

		// Get dir4 by the with lowest press time (besides 0 frames)
		int min_press_time = 0;
		for (int i = 2; i <= 8; i += 2) {
			if (dirpress[i] > 0) {
				if (min_press_time == 0 || dirpress[i] < min_press_time) {
					dir4 = i;
					min_press_time = dirpress[i];
				}
			}
		}

		// Dir8 will be at least equal to Dir4
		dir8 = dir4;

		// Check diagonal directions (There is a priority order)
		if		(dirpress[9] > 0)	dir8 = 9;
		else if (dirpress[7] > 0)	dir8 = 7;
		else if (dirpress[3] > 0)	dir8 = 3;
		else if (dirpress[1] > 0)	dir8 = 1;
	}
}

void Input_::ResetKeys() {
	triggered.reset();
	repeated.reset();
	released.reset();
	for (unsigned i = 0; i < BUTTON_COUNT; i++) {
		press_time[i] = 0;
	}
	dir4 = 0;
	dir8 = 0;

	DisplayUi->GetKeyStates().reset();
}

bool Input_::IsPressed(Input_::Button button) {
	WaitInput(true);
	return press_time[button] > 0;
}

bool Input_::IsTriggered(Input_::Button button) {
	WaitInput(true);
	return triggered[button];
}

bool Input_::IsRepeated(Input_::Button button) {
	WaitInput(true);
	return repeated[button];
}

bool Input_::IsReleased(Input_::Button button) {
	WaitInput(false);
	return released[button];
}

bool Input_::IsAnyPressed() {
	WaitInput(true);
	return std::find_if(press_time.begin(), press_time.end(),
						boost::lambda::_1 > 0) != press_time.end();
}

bool Input_::IsAnyTriggered() {
	WaitInput(true);
	return triggered.any();
}

bool Input_::IsAnyRepeated() {
	WaitInput(true);
	return repeated.any();
}

bool Input_::IsAnyReleased() {
	WaitInput(false);
	return released.any();
}

std::vector<Input_::Button> Input_::GetAllPressed() {
	WaitInput(true);
	std::vector<Input_::Button> vector;
	for (unsigned i = 0; i < BUTTON_COUNT; i++) {
		if (press_time[i] > 0)
			vector.push_back((Input_::Button)i);
	}
	return vector;
}

std::vector<Input_::Button> Input_::GetAllTriggered() {
	WaitInput(true);
	std::vector<Input_::Button> vector;
	for (unsigned i = 0; i < BUTTON_COUNT; i++) {
		if (triggered[i])
			vector.push_back((Input_::Button)i);
	}
	return vector;
}

std::vector<Input_::Button> Input_::GetAllRepeated() {
	WaitInput(true);
	std::vector<Input_::Button> vector;
	for (unsigned i = 0; i < BUTTON_COUNT; i++) {
		if (repeated[i])
			vector.push_back((Input_::Button)i);
	}
	return vector;
}

std::vector<Input_::Button> Input_::GetAllReleased() {
	WaitInput(false);
	std::vector<Input_::Button> vector;
	for (unsigned i = 0; i < BUTTON_COUNT; i++) {
		if (released[i])
			vector.push_back((Input_::Button)i);
	}
	return vector;
}
