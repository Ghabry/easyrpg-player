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
#include <algorithm>
#include <sstream>
#include <vector>
#include <map>
#include <iomanip>

#include "output.h"
#include "graphics.h"
#include "bitmap.h"
#include "bitmap_screen.h"
#include "cache.h"
#include "baseui.h"
#include "drawable.h"
#include "player.h"
#include "zobj.h"

#include <boost/bind.hpp>
#include <boost/math/special_functions/round.hpp>


unsigned Graphics_::SecondToFrame(float const second) {
	return(second * framerate);
}

Graphics_::Graphics_()
		: fps_on_screen(false)
		, drawable_id(0)
		, overlay_visible(true)
		, current_fps_(0)
		, framerate(DEFAULT_FPS)
		, framecount(0)
		, frame_interval(1000.0 / framerate)
		, next_fps_calculation_time(0)
		, fps_draw_counter(0)
		, expected_next_frame_end_time(-1) // needs FrameReset value
		, frozen_screen(BitmapScreen::Create())
		, black_screen(BitmapScreen::Create(
			Bitmap::Create(SCREEN_TARGET_WIDTH,
						   SCREEN_TARGET_HEIGHT,
						   Color(0, 0, 0, 255))))
		, frozen(false)
		, screen_erased(false)
		, drawable_creation(0)
		, state(EASYRPG_MAKE_SHARED<State>())
		, screen_buffer_(Bitmap::Create(SCREEN_TARGET_WIDTH,
										SCREEN_TARGET_HEIGHT,
										Color(0, 0, 0, 255)))
		, back_color(Color(0, 0, 0, 255))
{}

BitmapRef const& Graphics_::ScreenBuffer() const {
	return screen_buffer_;
}

Color const& Graphics_::GetBackcolor() const {
	return back_color;
}
void Graphics_::SetBackcolor(const Color &color) {
	back_color = color;
}

void Graphics_::CleanScreen() {
	screen_buffer_->fill(screen_buffer_->rect(), back_color);
}

void Graphics_::Update() {
	if (frozen) return;

	InternUpdate();
}

void Graphics_::UpdateTitle(double const fps) {
	current_fps_ = fps;

	if (DisplayUi->IsFullscreen()) return;

	std::ostringstream title(GAME_TITLE, std::ios::out | std::ios::ate);
	if (!fps_on_screen) {
		title << " - FPS " << std::setprecision(4) << current_fps_;
	}

	DisplayUi->SetTitle(title.str());
}

void Graphics_::DrawFrame() {
	++fps_draw_counter;

	if (transition_duration > 0) {
		UpdateTransition();
		return;
	}
	if (screen_erased) return;

	if (state->zlist_dirty) {
		state->zlist.sort(SortZObj);
		state->zlist_dirty = false;
	}

	CleanScreen();

	std::list<EASYRPG_SHARED_PTR<ZObj> >::iterator it_zlist;
	for (it_zlist = state->zlist.begin(); it_zlist != state->zlist.end(); it_zlist++) {
		Drawable* const d = (*it_zlist)->GetId();
		if(d->visible) { d->Draw((*it_zlist)->GetZ()); }
	}

	if (overlay_visible) {
		DrawOverlay();
	}

	DisplayUi->UpdateDisplay();
}

void Graphics_::DrawOverlay() {
	if (Graphics().fps_on_screen) {
		std::ostringstream text;
		text << "FPS: " << std::setprecision(4) << current_fps_;
		DisplayUi->DrawScreenText(text.str());
	}
}

BitmapRef Graphics_::SnapToBitmap() {
	CleanScreen();

	std::list<EASYRPG_SHARED_PTR<ZObj> >::iterator it_zlist;
	for (it_zlist = state->zlist.begin(); it_zlist != state->zlist.end(); it_zlist++) {
		Drawable* const d = (*it_zlist)->GetId();
		if(d->visible) { d->Draw((*it_zlist)->GetZ()); }
	}

	return Bitmap::Create(screen_buffer_);
}

void Graphics_::Freeze() {
	frozen_screen->SetBitmap(SnapToBitmap());
	frozen = true;
}

void Graphics_::Transition(TransitionType type, int duration, bool erase) {
	if (erase && screen_erased) return;

	if (type != TransitionNone) {
		transition_type = type;
		transition_frame = 0;
		transition_duration = type == TransitionErase ? 1 : duration;

		if (state->zlist_dirty) {
			state->zlist.sort(SortZObj);
			state->zlist_dirty = false;
		}

		if (!frozen) Freeze();

		if (erase) {
			screen1 = frozen_screen;

			screen2 = black_screen;
		} else {
			screen2 = frozen_screen;

			if (screen_erased)
				screen1 = black_screen;
			else
				screen1 = screen2;
		}

		for (int i = 1; i <= transition_duration; i++) {
			Player::update();
			InternUpdate();
		}
	}

	if (!erase) frozen_screen->SetBitmap(BitmapRef());

	frozen = false;
	screen_erased = erase;

	transition_duration = 0;

	FrameReset();
}

void Graphics_::UpdateTransition() {
	// FIXME: Comments. Pleeeease. screen1, screen2?
	int w = DisplayUi->GetWidth();
	int h = DisplayUi->GetHeight();

	transition_frame++;

	int percentage = transition_frame * 100 / transition_duration;

	if (!screen1 || !screen2) {
		return;
	}

	switch (transition_type) {
	case TransitionFadeIn:
		screen1->BlitScreen(0, 0);
		screen2->SetOpacityEffect(255 * percentage / 100);
		screen2->BlitScreen(0, 0);
		break;
	case TransitionFadeOut:
		screen1->BlitScreen(0, 0);
		screen2->SetOpacityEffect(255 * percentage / 100);
		screen2->BlitScreen(0, 0);
		break;
	case TransitionRandomBlocks:
		break;
	case TransitionRandomBlocksUp:
		break;
	case TransitionRandomBlocksDown:
		break;
	case TransitionBlindOpen:
		for (int i = 0; i < h / 8; i++) {
			screen1->BlitScreen(0, i * 8, Rect(0, i * 8, w, 8 - 8 * percentage / 100));
			screen2->BlitScreen(0, i * 8 + 8 - 8 * percentage / 100, Rect(0, i * 8 + 8 - 8 * percentage / 100, w, 8 * percentage / 100));
		}
		break;
	case TransitionBlindClose:
		for (int i = 0; i < h / 8; i++) {
			screen1->BlitScreen(0, i * 8 + 8 * percentage / 100, Rect(0, i * 8 + 8 * percentage / 100, w, 8 - 8 * percentage / 100));
			screen2->BlitScreen(0, i * 8, Rect(0, i * 8, w, 8 * percentage / 100));
		}
		break;
	case TransitionVerticalStripesIn:
	case TransitionVerticalStripesOut:
		for (int i = 0; i < h / 6 + 1 - h / 6 * percentage / 100; i++) {
			screen1->BlitScreen(0, i * 6 + 3, Rect(0, i * 6 + 3, w, 3));
			screen1->BlitScreen(0, h - i * 6, Rect(0, h - i * 6, w, 3));
		}
		for (int i = 0; i < h / 6 * percentage / 100; i++) {
			screen2->BlitScreen(0, i * 6, Rect(0, i * 6, w, 3));
			screen2->BlitScreen(0, h - 3 - i * 6, Rect(0, h - 3 - i * 6, w, 3));
		}
		break;
	case TransitionHorizontalStripesIn:
	case TransitionHorizontalStripesOut:
		for (int i = 0; i < w / 8 + 1 - w / 8 * percentage / 100; i++) {
			screen1->BlitScreen(i * 8 + 4, 0, Rect(i * 8 + 4, 0, 4, h));
			screen1->BlitScreen(w  - i * 8, 0, Rect(w - i * 8, 0, 4, h));
		}
		for (int i = 0; i < w / 8 * percentage / 100; i++) {
			screen2->BlitScreen(i * 8, 0, Rect(i * 8, 0, 4, h));
			screen2->BlitScreen(w - 4 - i * 8, 0, Rect(w - 4 - i * 8, 0, 4, h));
		}
		break;
	case TransitionBorderToCenterIn:
	case TransitionBorderToCenterOut:
		screen2->BlitScreen(0, 0);
		screen1->BlitScreen((w / 2) * percentage / 100, (h / 2) * percentage / 100, Rect((w / 2) * percentage / 100, (h / 2) * percentage / 100, w - w * percentage / 100, h - h * percentage / 100));
		break;
	case TransitionCenterToBorderIn:
	case TransitionCenterToBorderOut:
		screen1->BlitScreen(0, 0);
		screen2->BlitScreen(w / 2 - (w / 2) * percentage / 100, h / 2 - (h / 2) * percentage / 100, Rect(w / 2 - (w / 2) * percentage / 100, h / 2 - (h / 2) * percentage / 100, w * percentage / 100, h * percentage / 100));
		break;
	case TransitionScrollUpIn:
	case TransitionScrollUpOut:
		screen1->BlitScreen(0, -h * percentage / 100);
		screen2->BlitScreen(0, h - h * percentage / 100);
		break;
	case TransitionScrollDownIn:
	case TransitionScrollDownOut:
		screen1->BlitScreen(0, h * percentage / 100);
		screen2->BlitScreen(0, -h + h * percentage / 100);
		break;
	case TransitionScrollLeftIn:
	case TransitionScrollLeftOut:
		screen1->BlitScreen(-w * percentage / 100, 0);
		screen2->BlitScreen(w - w * percentage / 100, 0);
		break;
	case TransitionScrollRightIn:
	case TransitionScrollRightOut:
		screen1->BlitScreen(w * percentage / 100, 0);
		screen2->BlitScreen(-w + w * percentage / 100, 0);
		break;
	case TransitionVerticalCombine:
		screen1->BlitScreen(0, (h / 2) * percentage / 100, Rect(0, (h / 2) * percentage / 100, w, h - h * percentage / 100));
		screen2->BlitScreen(0, -h / 2 + (h / 2) * percentage / 100, Rect(0, 0, w, h / 2));
		screen2->BlitScreen(0, h - (h / 2) * percentage / 100, Rect(0, h / 2, w, h / 2));
		break;
	case TransitionVerticalDivision:
		screen1->BlitScreen(0, -(h / 2) * percentage / 100, Rect(0, 0, w, h / 2));
		screen1->BlitScreen(0, h / 2 + (h / 2) * percentage / 100, Rect(0, h / 2, w, h / 2));
		screen2->BlitScreen(0, h / 2 - (h / 2) * percentage / 100, Rect(0, h / 2 - (h / 2) * percentage / 100, w, h * percentage / 100));
		break;
	case TransitionHorizontalCombine:
		screen1->BlitScreen((w / 2) * percentage / 100, 0, Rect((w / 2) * percentage / 100, 0, w - w * percentage / 100, h));
		screen2->BlitScreen(- w / 2 + (w / 2) * percentage / 100, 0, Rect(0, 0, w / 2, h));
		screen2->BlitScreen(w - (w / 2) * percentage / 100, 0, Rect(w / 2, 0, w / 2, h));
		break;
	case TransitionHorizontalDivision:
		screen1->BlitScreen(-(w / 2) * percentage / 100, 0, Rect(0, 0, w / 2, h));
		screen1->BlitScreen(w / 2 + (w / 2) * percentage / 100, 0, Rect(w / 2, 0, w / 2, h));
		screen2->BlitScreen(w / 2 - (w / 2) * percentage / 100, 0, Rect(w / 2 - (w / 2) * percentage / 100, 0, w * percentage / 100, h));
		break;
	case TransitionCrossCombine:
		screen1->BlitScreen((w / 2) * percentage / 100, 0, Rect((w / 2) * percentage / 100, 0, w - w * percentage / 100, (h / 2) * percentage / 100));
		screen1->BlitScreen((w / 2) * percentage / 100, h - (h / 2) * percentage / 100, Rect((w / 2) * percentage / 100, h - (h / 2) * percentage / 100, w - w * percentage / 100, (h / 2) * percentage / 100));
		screen1->BlitScreen(0, (h / 2) * percentage / 100, Rect(0, (h / 2) * percentage / 100, w, h - h * percentage / 100));
		screen2->BlitScreen(- w / 2 + (w / 2) * percentage / 100, -h / 2 + (h / 2) * percentage / 100, Rect(0, 0, w / 2, h / 2));
		screen2->BlitScreen(w - (w / 2) * percentage / 100, -h / 2 + (h / 2) * percentage / 100, Rect(w / 2, 0, w / 2, h / 2));
		screen2->BlitScreen(w - (w / 2) * percentage / 100, h - (h / 2) * percentage / 100, Rect(w / 2, h / 2, w / 2, h / 2));
		screen2->BlitScreen(- w / 2 + (w / 2) * percentage / 100, h - (h / 2) * percentage / 100, Rect(0, h / 2, w / 2, h / 2));
		break;
	case TransitionCrossDivision:
		screen1->BlitScreen(-(w / 2) * percentage / 100, -(h / 2) * percentage / 100, Rect(0, 0, w / 2, h / 2));
		screen1->BlitScreen(w / 2 + (w / 2) * percentage / 100, -(h / 2) * percentage / 100, Rect(w / 2, 0, w / 2, h / 2));
		screen1->BlitScreen(w / 2 + (w / 2) * percentage / 100, h / 2 + (h / 2) * percentage / 100, Rect(w / 2, h / 2, w / 2, h / 2));
		screen1->BlitScreen(-(w / 2) * percentage / 100, h / 2 + (h / 2) * percentage / 100, Rect(0, h / 2, w / 2, h / 2));
		screen2->BlitScreen(w / 2 - (w / 2) * percentage / 100, 0, Rect(w / 2 - (w / 2) * percentage / 100, 0, w * percentage / 100, h / 2 - (h / 2) * percentage / 100));
		screen2->BlitScreen(w / 2 - (w / 2) * percentage / 100, h / 2 + (h / 2) * percentage / 100, Rect(w / 2 - (w / 2) * percentage / 100, h / 2 + (h / 2) * percentage / 100, w * percentage / 100, h / 2 + (h / 2) * percentage / 100));
		screen2->BlitScreen(0, h / 2 - (h / 2) * percentage / 100, Rect(0, h / 2 - (h / 2) * percentage / 100, w, h * percentage / 100));
		break;
	case TransitionZoomIn:
		break;
	case TransitionZoomOut:
		break;
	case TransitionMosaicIn:
		break;
	case TransitionMosaicOut:
		break;
	case TransitionWaveIn:
		break;
	case TransitionWaveOut:
		break;
	default:
		CleanScreen();
		break;
	}

	DisplayUi->UpdateDisplay();
}

void Graphics_::FrameReset() {
	unsigned const current_time = DisplayUi->GetTicks();

	next_fps_calculation_time = current_time + 1000;
	fps_draw_counter = 0;
	expected_next_frame_end_time = current_time + frame_interval;

	frame_interval = 1000.0 / framerate;
}

void Graphics_::InternUpdate() {
	if(expected_next_frame_end_time < 0) {
		FrameReset(); // init frame variables
		UpdateTitle(0);
	}

	// draw screen if enough time left
	unsigned const frame_start_time = DisplayUi->GetTicks();
	if(frame_start_time < expected_next_frame_end_time) {
		DrawFrame();
		mrb_garbage_collect(Player::current_vm());
	}
	unsigned const frame_end_time = DisplayUi->GetTicks();

	// calculate FPS if passed FPS calculation time
	if(frame_end_time >= next_fps_calculation_time) {
		// check critical frame rate drop
		if(fps_draw_counter == 0) {
			Output().Debug("Critical frame rate drop.(Cannot draw a single frame in this second)");
		}

		double const base = frame_end_time - next_fps_calculation_time + 1000.0;
		UpdateTitle(fps_draw_counter * 1000.0 / base);
		FrameReset();
	}

	// sleep if time left
	namespace policies = boost::math::policies;
	int const sleep_time = boost::math::round(
		expected_next_frame_end_time - frame_end_time,
		policies::policy<policies::rounding_error<policies::ignore_error> >());
	if(sleep_time > 0) { DisplayUi->Sleep(sleep_time); }

	// update next frame end time
	expected_next_frame_end_time += frame_interval;
}

void Graphics_::Wait(int duration) {
	while(duration-- > 0) {
		Update();
	}
}

int Graphics_::GetFrameCount() {
	return framecount;
}
void Graphics_::SetFrameCount(int nframecount) {
	framecount = nframecount;
}

int Graphics_::GetFrameRate() const {
	return framerate;
}
void Graphics_::SetFrameRate(int v) {
	framerate = v;
}

ZObj* Graphics_::RegisterZObj(int z, Drawable* ID) {
	state->zlist.push_back(EASYRPG_MAKE_SHARED<ZObj>(z, drawable_creation++, ID));
	state->zlist_dirty = true;
	return state->zlist.back().get();
}

void Graphics_::RegisterZObj(int z, Drawable* ID, bool /* multiz */) {
	state->zlist.push_back(EASYRPG_MAKE_SHARED<ZObj>(z, 999999, ID));
	state->zlist_dirty = true;
}

static bool check_id(EASYRPG_SHARED_PTR<ZObj> const& z, Drawable* const ID) {
	return z->GetId() == ID;
}

void Graphics_::RemoveZObj(Drawable* ID, bool) {
	state->zlist.remove_if(boost::bind(&check_id, _1, ID));
}

void Graphics_::UpdateZObj(ZObj* zobj, int z) {
	zobj->SetZ(z);
	state->zlist_dirty = true;
}

inline bool Graphics_::SortZObj(EASYRPG_SHARED_PTR<ZObj> const& first, EASYRPG_SHARED_PTR<ZObj> const& second) {
	if (first->GetZ() < second->GetZ()) return true;
	else if (first->GetZ() > second->GetZ()) return false;
	else return first->GetCreation() < second->GetCreation();
}

void Graphics_::Push() {
	stack.push_back(state);
	state.reset(new State());
}

void Graphics_::Pop() {
	if (stack.size() > 0) {
		state = stack.back();
		stack.pop_back();
	}
}
