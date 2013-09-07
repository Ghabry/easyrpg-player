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

#ifdef USE_SDL

// Headers
#include "sdl_ui.h"
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#ifndef NOMINMAX
	#define NOMINMAX
	#endif
	#include <windows.h>
	#include "SDL_syswm.h"
#elif GEKKO
	#include <gccore.h>
	#include <wiiuse/wpad.h>
#endif
#include "color.h"
#include "graphics.h"
#include "keys.h"
#include "output.h"
#include "player.h"
#include "bitmap.h"
#include "audio.h"
#include "sdl_audio.h"
#include "utils.h"
#include "matrix.h"

#include <bitset>
#include <cstdlib>
#include <cstring>
#include <SDL.h>
#include <pixman.h>
#include <boost/format.hpp>

AudioInterface& SdlUi::GetAudio() {
	return *audio_;
}

static int FilterUntilFocus(const SDL_Event* evnt);

#if defined(USE_KEYBOARD) && defined(SUPPORT_KEYBOARD)
	static Keys::InputKey SdlKey2InputKey(SDLKey sdlkey);
#endif

#if defined(USE_JOYSTICK) && defined(SUPPORT_JOYSTICK)
	static Keys::InputKey SdlJKey2InputKey(int button_index);
#endif

#ifdef GEKKO
	static void GekkoResetCallback();
#endif

SdlUi::SdlUi(long width, long height, const std::string& title, bool fs_flag) :
	zoom_available(true),
	toggle_fs_available(false),
	mode_changing(false),
	screen(NULL) {

#ifdef GEKKO
	WPAD_Init();

	SYS_SetResetCallback(GekkoResetCallback);
#endif

	uint32_t flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;
#if (!defined(NDEBUG) || defined(_WIN32))
	flags |= SDL_INIT_NOPARACHUTE;
#endif

	// Set some SDL env. variables before starting
	// These are platform dependant, so every port
	// needs to set them manually
#ifdef _WIN32
	// Tell SDL to use DirectDraw port
	// in release mode
#ifndef DEBUG
	//putenv("SDL_VIDEODRIVER=directx"); // Disables Vsync and Aero under Vista and higher
	putenv("SDL_AUDIODRIVER=dsound");
#endif

	// Set window position to the middle of the
	// screen
	putenv("SDL_VIDEO_WINDOW_POS=center");
#elif defined(PSP)
	putenv("SDL_ASPECT_RATIO=4:3");
#endif

	if (SDL_Init(flags) < 0) {
		Output().Error(boost::format("couldn't initialize SDL: %s") % SDL_GetError());
	}

	SetAppIcon();

	BeginDisplayModeChange();
		if (!RequestVideoMode(width, height, fs_flag)) {
			Output().Error("No suitable video resolution found. Aborting.");
		}
	EndDisplayModeChange();

	SetTitle(title);

#if (defined(USE_JOYSTICK) && defined(SUPPORT_JOYSTICK)) || (defined(USE_JOYSTICK_AXIS) && defined(SUPPORT_JOYSTICK_AXIS)) || (defined(USE_JOYSTICK_HAT) && defined(SUPPORT_JOYSTICK_HAT))
	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) {
		Output().Warning(boost::format("couldn't initialize joystick: %s") % SDL_GetError());
	}

	SDL_JoystickEventState(1);
	SDL_JoystickOpen(0);
#endif

#if defined(USE_MOUSE) && defined(SUPPORT_MOUSE)
	ShowCursor(true);
#else
	ShowCursor(false);
#endif

#if defined(HAVE_SDL_MIXER)
	audio_.reset(new SdlAudio());
#elif defined(HAVE_OPENAL)
	audio_.reset(new ALAudio());
#else
	audio_.reset(new EmptyAudio());
#endif
}

SdlUi::~SdlUi() {
#if defined(GPH)
	chdir("/usr/gp2x");
	execl("./gp2xmenu", "./gp2xmenu", NULL);
#else
	SDL_Quit();
#endif
}

uint32_t SdlUi::GetTicks() const {
	return SDL_GetTicks();
}

void SdlUi::Sleep(uint32_t time) {
	SDL_Delay(time);
}

bool SdlUi::RequestVideoMode(int width, int height, bool fullscreen) {
	// FIXME: Split method into submethods, really, this method isn't nice.
	// Note to Zhek, don't delete this fixme again.

	const SDL_VideoInfo *vinfo;
	SDL_Rect **modes;
	uint32_t flags = SDL_SWSURFACE;

	vinfo = SDL_GetVideoInfo();

	current_display_mode.height = height;
	current_display_mode.width = width;

	if (vinfo->wm_available) {
		toggle_fs_available = true;
		// FIXME: this for may work, but is really confusing. Calling a method
		// that does this with the desired flags would be nicer.
		for (;;) {
			if (fullscreen) {
				flags |= SDL_FULLSCREEN;
			}

			modes = SDL_ListModes(NULL, flags);
			if (modes != NULL) {
				// Set up...
				current_display_mode.flags = flags;

				if (modes == (SDL_Rect **)-1) {
					// All modes available
					// If we have a high res, turn zoom on
// FIXME: Detect SDL version for this. current_h and current_w are only available in >1.2.10
// PSP SDL port is older than this, lol
#ifndef PSP
					current_display_mode.zoom = (vinfo->current_h > height*2 && vinfo->current_w > width*2);
#endif
#if defined(SUPPORT_ZOOM)
					zoom_available = current_display_mode.zoom;
#else
					zoom_available = false;
#endif
					return true;
				} else {
					int len = 0;
					while (modes[len])
						++len;

					for (int i = len-1; i >= 0; --i) {
						if (
							(modes[i]->h == height && modes[i]->w == width)
#if defined(SUPPORT_ZOOM)
							|| (modes[i]->h == height*2 && modes[i]->w == width*2)
#endif
						) {
							current_display_mode.zoom = ((modes[i]->w >> 1) == width);
							zoom_available = current_display_mode.zoom;
							return true;
						}
					}
				}
			}
			// No modes available
			if ((flags & SDL_FULLSCREEN) == SDL_FULLSCREEN) {
				// Try without fullscreen
				flags &= ~SDL_FULLSCREEN;
			} else {
				// No mode available :(
				return false;
			}
		}
	} // wm_available

	if (!fullscreen) {
		// Stop here since we need a window manager for non fullscreen modes
		return false;
	}

	// No hard accel and no window manager
	flags = SDL_SWSURFACE | SDL_FULLSCREEN;

	modes = SDL_ListModes(NULL, flags);
	if (modes == NULL) {
		// No video for you
		return false;
	}

	if (modes == (SDL_Rect **)-1) {
		// All modes available
		current_display_mode.flags = flags;
		current_display_mode.zoom = false;
		zoom_available = current_display_mode.zoom;
		return true;
	}

	int len = 0;
	while (modes[len])
		++len;

	for (int i = len-1; i > 0; --i) {
		if ((modes[i]->h == height && modes[i]->w == width)
#if defined(SUPPORT_ZOOM)
			|| (modes[i]->h == height*2 && modes[i]->w == width*2)
#endif
			) {
				current_display_mode.flags = flags;
				// FIXME: we have to find a way to make zoom possible only in windowed mode
				current_display_mode.zoom = ((modes[i]->w >> 1) == width);
				zoom_available = current_display_mode.zoom;
				return true;
		}
	}

	// Didn't find a suitable video mode
	return false;
}

void SdlUi::BeginDisplayModeChange() {
	last_display_mode = current_display_mode;
	current_display_mode.effective = false;
	mode_changing = true;
}

void SdlUi::EndDisplayModeChange() {
	// Check if the new display mode is different from last one
	if (mode_changing && (
		current_display_mode.flags != last_display_mode.flags ||
		current_display_mode.zoom != last_display_mode.zoom ||
		current_display_mode.width != last_display_mode.width ||
		current_display_mode.height != last_display_mode.height)) {

			if (!RefreshDisplayMode()) {
				// Mode change failed, check if last one was effective
				if (last_display_mode.effective) {
					current_display_mode = last_display_mode;

					// Try a rollback to last mode
					if (!RefreshDisplayMode()) {
						Output().Error(boost::format("couldn't rollback to last display mode: %s") % SDL_GetError());
					}
				} else {
					Output().Error(boost::format("couldn't set display mode: %s") % SDL_GetError());
				}
			}

			current_display_mode.effective = true;

			mode_changing = false;
	}
}

bool SdlUi::RefreshDisplayMode() {
	uint32_t flags = current_display_mode.flags;
	int display_width = current_display_mode.width;
	int display_height = current_display_mode.height;
	int bpp = current_display_mode.bpp;

	// Display on screen fps while fullscreen or no window available
	Graphics().fps_on_screen = (flags & SDL_FULLSCREEN) == SDL_FULLSCREEN || !toggle_fs_available;

	if (zoom_available && current_display_mode.zoom) {
		display_width *= 2;
		display_height *= 2;
	}

	// Create our window
	screen = SDL_SetVideoMode(display_width, display_height, bpp, flags);

	if (!screen)
		return false;

	// Modes below 15 bpp aren't supported
	if (screen->format->BitsPerPixel < 15)
		return false;

	current_display_mode.bpp = screen->format->BitsPerPixel;

	return true;
}

#ifdef SUPPORT_FULL_SCALING
void SdlUi::Resize(long width, long height) {
	if (mode_changing) {
		current_display_mode.width = width;
		current_display_mode.height = height;
	}
}
#else
void SdlUi::Resize(long /*width*/, long /*height*/) {
}
#endif

void SdlUi::ToggleFullscreen() {
	if (toggle_fs_available && mode_changing) {
		if ((current_display_mode.flags & SDL_FULLSCREEN) == SDL_FULLSCREEN)
			current_display_mode.flags &= ~SDL_FULLSCREEN;
		else
			current_display_mode.flags |= SDL_FULLSCREEN;
	}
}

void SdlUi::ToggleZoom() {
	if (zoom_available && mode_changing) {
		current_display_mode.zoom = !current_display_mode.zoom;
	}
}

void SdlUi::ProcessEvents() {
	SDL_Event evnt;

	// Poll SDL events and process them
	while (SDL_PollEvent(&evnt)) {
		ProcessEvent(evnt);

		if (Player::exit_flag())
			break;
	}
}

static pixman_format_code_t to_pixman_format(SDL_PixelFormat const& f) {
	Uint8 const shifts[3] = { f.Rshift, f.Gshift, f.Bshift };
	Uint8 const max_shift = *std::max_element(shifts, shifts + 3);
	int const alpha_count = std::bitset<32>((int32_t)f.Amask).count();

	int t =
			(f.Rshift > f.Gshift && f.Gshift > f.Bshift)
			? (alpha_count > 0 && max_shift < f.Ashift
			   ? PIXMAN_TYPE_ARGB : PIXMAN_TYPE_RGBA):
			(f.Rshift < f.Gshift && f.Gshift < f.Bshift)
			? (alpha_count > 0  && max_shift < f.Ashift
			   ? PIXMAN_TYPE_ABGR : PIXMAN_TYPE_BGRA):
			-1; // invalid type

	assert(t != -1);

	/*
	if((f.BytesPerPixel % 2 == 0) and not Utils::IsBigEndian()) {
		switch(t) {
			case PIXMAN_TYPE_ARGB: t = PIXMAN_TYPE_BGRA; break;
			case PIXMAN_TYPE_ABGR: t = PIXMAN_TYPE_RGBA; break;
			case PIXMAN_TYPE_RGBA: t = PIXMAN_TYPE_ABGR; break;
			case PIXMAN_TYPE_BGRA: t = PIXMAN_TYPE_ARGB; break;
		}
	}
	*/

	return
			pixman_format_code_t(PIXMAN_FORMAT(
				f.BitsPerPixel, t, alpha_count,
				std::bitset<32>((int32_t)f.Rmask).count(),
				std::bitset<32>((int32_t)f.Gmask).count(),
				std::bitset<32>((int32_t)f.Bmask).count()));
}

void SdlUi::UpdateDisplay() {
	if (SDL_MUSTLOCK(screen) && SDL_LockSurface(screen) < 0) {
		return;
	}

	pixman_image_ptr const img(
		pixman_image_create_bits_no_clear(
			to_pixman_format(*screen->format), screen->w, screen->h,
			reinterpret_cast<uint32_t*>(screen->pixels),
			screen->pitch),
		&pixman_image_unref);
	BOOST_VERIFY(pixman_image_set_filter(img.get(), PIXMAN_FILTER_FAST, NULL, 0));

	BitmapRef const& buffer = Graphics().ScreenBuffer();

	SET_MATRIX(buffer->image(), Matrix::scale_(
		buffer->width () / double(screen->w),
		buffer->height() / double(screen->h)));

	pixman_image_composite(PIXMAN_OP_SRC, buffer->image(), NULL, img.get(),
						   0, 0, 0, 0, 0, 0, screen->w, screen->h);

	if (SDL_MUSTLOCK(screen)) {
		SDL_UnlockSurface(screen);
	}

	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

void SdlUi::SetTitle(const std::string &title) {
	SDL_WM_SetCaption(title.c_str(), NULL);
}

bool SdlUi::ShowCursor(bool flag) {
	bool temp_flag = cursor_visible;
	cursor_visible = flag;
	SDL_ShowCursor(flag ? SDL_ENABLE : SDL_DISABLE);
	return temp_flag;
}

void SdlUi::ProcessEvent(SDL_Event &evnt) {
	switch (evnt.type) {
		case SDL_ACTIVEEVENT:
			ProcessActiveEvent(evnt);
			return;

		case SDL_QUIT:
			Player::exit_flag(true);
			return;

		case SDL_KEYDOWN:
			ProcessKeyDownEvent(evnt);
			return;

		case SDL_KEYUP:
			ProcessKeyUpEvent(evnt);
			return;

		case SDL_MOUSEMOTION:
			ProcessMouseMotionEvent(evnt);
			return;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			ProcessMouseButtonEvent(evnt);
			return;

		case SDL_JOYBUTTONDOWN:
		case SDL_JOYBUTTONUP:
			ProcessJoystickButtonEvent(evnt);
			return;

		case SDL_JOYHATMOTION:
			ProcessJoystickHatEvent(evnt);
			return;

		case SDL_JOYAXISMOTION:
			ProcessJoystickAxisEvent(evnt);
			return;
	}
}

void SdlUi::ProcessActiveEvent(SDL_Event &evnt) {
#ifdef PAUSE_GAME_WHEN_FOCUS_LOST
	switch(evnt.active.state) {
		case SDL_APPINPUTFOCUS:
			if (!evnt.active.gain) {
#ifdef _WIN32
				// Prevent the player from hanging when it receives a
				// focus changed event but actually has focus.
				// This happens when a MsgBox appears.
				if (GetActiveWindow() != NULL) {
					return;
				}
#endif

				Player::pause();

				bool last = ShowCursor(true);

				// Filter SDL events with FilterUntilFocus until focus is
				// regained
				SDL_SetEventFilter(&FilterUntilFocus);
				SDL_WaitEvent(NULL);
				SDL_SetEventFilter(NULL);

				ShowCursor(last);

				ResetKeys();

				Player::resume();
			}
			return;
#endif

#if defined(USE_MOUSE) && defined(SUPPORT_MOUSE)
		case SDL_APPMOUSEFOCUS:
			mouse_focus = evnt.active.gain == 1;
			return;
#endif
	}
}

void SdlUi::ProcessKeyDownEvent(SDL_Event &evnt) {
#if defined(USE_KEYBOARD) && defined(SUPPORT_KEYBOARD)
	switch (evnt.key.keysym.sym) {
	case SDLK_F4:
		// Close program on LeftAlt+F4
		if (evnt.key.keysym.mod & KMOD_LALT) {
			Player::exit_flag(true);
			return;
		}

		// Toggle fullscreen on F4 and no alt is pressed
		if (!(evnt.key.keysym.mod & KMOD_RALT) && !(evnt.key.keysym.mod & KMOD_LALT)) {
			BeginDisplayModeChange();
				ToggleFullscreen();
			EndDisplayModeChange();
		}
		return;

	case SDLK_F5:
		// Toggle fullscreen on F5
		BeginDisplayModeChange();
			ToggleZoom();
		EndDisplayModeChange();
		return;

	case SDLK_F12:
		// Reset the game engine on F12
		Player::reset_flag(true);
		return;

	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		// Toggle fullscreen on Alt+Enter
		if (evnt.key.keysym.mod & KMOD_LALT || (evnt.key.keysym.mod & KMOD_RALT)) {
			BeginDisplayModeChange();
				ToggleFullscreen();
			EndDisplayModeChange();
			return;
		}

		// Continue if return/enter not handled by fullscreen hotkey
	default:
		// Update key state
		keys[SdlKey2InputKey(evnt.key.keysym.sym)] = true;
		return;
	}
#endif
}

void SdlUi::ProcessKeyUpEvent(SDL_Event &evnt) {
#if defined(USE_KEYBOARD) && defined(SUPPORT_KEYBOARD)
	keys[SdlKey2InputKey(evnt.key.keysym.sym)] = false;
#endif
}

void SdlUi::ProcessMouseMotionEvent(SDL_Event& /* evnt */) {
#if defined(USE_MOUSE) && defined(SUPPORT_MOUSE)
	mouse_focus = true;
	mouse_x = evnt.motion.x;
	mouse_y = evnt.motion.y;
#endif
}

void SdlUi::ProcessMouseButtonEvent(SDL_Event& /* evnt */) {
#if defined(USE_MOUSE) && defined(SUPPORT_MOUSE)
	switch (evnt.button.button) {
	case SDL_BUTTON_LEFT:
		keys[Keys::MOUSE_LEFT] = evnt.button.state == SDL_PRESSED;
		break;
	case SDL_BUTTON_MIDDLE:
		keys[Keys::MOUSE_MIDDLE] = evnt.button.state == SDL_PRESSED;
		break;
	case SDL_BUTTON_RIGHT:
		keys[Keys::MOUSE_RIGHT] = evnt.button.state == SDL_PRESSED;
		break;
	}
#endif
}

void SdlUi::ProcessJoystickButtonEvent(SDL_Event &evnt) {
#if defined(USE_JOYSTICK) && defined(SUPPORT_JOYSTICK)
	keys[SdlJKey2InputKey(evnt.jbutton.button)] = evnt.jbutton.state == SDL_PRESSED;
#endif
}

void SdlUi::ProcessJoystickHatEvent(SDL_Event &evnt) {
#if defined(USE_JOYSTICK_HAT)  && defined(SUPPORT_JOYSTICK_HAT)
	// Set all states to false
	keys[Keys::JOY_HAT_LOWER_LEFT] = false;
	keys[Keys::JOY_HAT_DOWN] = false;
	keys[Keys::JOY_HAT_LOWER_RIGHT] = false;
	keys[Keys::JOY_HAT_LEFT] = false;
	keys[Keys::JOY_HAT_RIGHT] = false;
	keys[Keys::JOY_HAT_UPPER_LEFT] = false;
	keys[Keys::JOY_HAT_UP] = false;
	keys[Keys::JOY_HAT_UPPER_RIGHT] = false;

	// Check hat states
	if ((evnt.jhat.value & SDL_HAT_RIGHTUP) == SDL_HAT_RIGHTUP)
		keys[Keys::JOY_HAT_UPPER_RIGHT] = true;

	else if ((evnt.jhat.value & SDL_HAT_RIGHTDOWN)  == SDL_HAT_RIGHTDOWN)
		keys[Keys::JOY_HAT_LOWER_RIGHT] = true;

	else if ((evnt.jhat.value & SDL_HAT_LEFTUP)  == SDL_HAT_LEFTUP)
		keys[Keys::JOY_HAT_UPPER_LEFT] = true;

	else if ((evnt.jhat.value & SDL_HAT_LEFTDOWN)  == SDL_HAT_LEFTDOWN)
		keys[Keys::JOY_HAT_LOWER_LEFT] = true;

	else if (evnt.jhat.value & SDL_HAT_UP)
		keys[Keys::JOY_HAT_UP] = true;

	else if (evnt.jhat.value & SDL_HAT_RIGHT)
		keys[Keys::JOY_HAT_RIGHT] = true;

	else if (evnt.jhat.value & SDL_HAT_DOWN)
		keys[Keys::JOY_HAT_DOWN] = true;

	else if (evnt.jhat.value & SDL_HAT_LEFT)
		keys[Keys::JOY_HAT_LEFT] = true;
#endif
}

void SdlUi::ProcessJoystickAxisEvent(SDL_Event &evnt) {
#if defined(USE_JOYSTICK_AXIS)  && defined(SUPPORT_JOYSTICK_AXIS)
	// Horizontal axis
	if (evnt.jaxis.axis == 0) {
		if (evnt.jaxis.value < -JOYSTICK_AXIS_SENSIBILITY) {
			keys[Keys::JOY_AXIS_X_LEFT] = true;
			keys[Keys::JOY_AXIS_X_RIGHT] = false;
		} else if (evnt.jaxis.value > JOYSTICK_AXIS_SENSIBILITY) {
			keys[Keys::JOY_AXIS_X_LEFT] = false;
			keys[Keys::JOY_AXIS_X_RIGHT] = true;
		} else {
			keys[Keys::JOY_AXIS_X_LEFT] = false;
			keys[Keys::JOY_AXIS_X_RIGHT] = false;
		}

	// Vertical Axis
	} else if (evnt.jaxis.axis == 1) {
		if (evnt.jaxis.value < -JOYSTICK_AXIS_SENSIBILITY) {
			keys[Keys::JOY_AXIS_Y_UP] = true;
			keys[Keys::JOY_AXIS_Y_DOWN] = false;
		} else if (evnt.jaxis.value > JOYSTICK_AXIS_SENSIBILITY) {
			keys[Keys::JOY_AXIS_Y_UP] = false;
			keys[Keys::JOY_AXIS_Y_DOWN] = true;
		} else {
			keys[Keys::JOY_AXIS_Y_UP] = false;
			keys[Keys::JOY_AXIS_Y_DOWN] = false;
		}
	}
#endif
}

void SdlUi::SetAppIcon() {
#ifdef _WIN32
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version)

	if (SDL_GetWMInfo(&wminfo) < 0)
		Output().Error("Wrong SDL version");

	HINSTANCE handle = GetModuleHandle(NULL);
	HICON icon = LoadIcon(handle, MAKEINTRESOURCE(23456));

	if (icon == NULL)
		Output().Error("Couldn't load icon.");

	SetClassLongPtr(wminfo.window, GCLP_HICON, (LONG_PTR) icon);
#endif
}

void SdlUi::ResetKeys() {
	for (size_t i = 0; i < keys.size(); i++) {
		keys[i] = false;
	}
}

bool SdlUi::IsFullscreen() {
	return (current_display_mode.flags & SDL_FULLSCREEN) == SDL_FULLSCREEN;
}

#if defined(USE_KEYBOARD) && defined(SUPPORT_KEYBOARD)
Keys::InputKey SdlKey2InputKey(SDLKey sdlkey) {
	switch (sdlkey) {
		case SDLK_BACKSPACE		: return Keys::BACKSPACE;
		case SDLK_TAB			: return Keys::TAB;
		case SDLK_CLEAR			: return Keys::CLEAR;
		case SDLK_RETURN		: return Keys::RETURN;
		case SDLK_PAUSE			: return Keys::PAUSE;
		case SDLK_ESCAPE		: return Keys::ESCAPE;
		case SDLK_SPACE			: return Keys::SPACE;
		case SDLK_PAGEUP		: return Keys::PGUP;
		case SDLK_PAGEDOWN		: return Keys::PGDN;
		case SDLK_END			: return Keys::ENDS;
		case SDLK_HOME			: return Keys::HOME;
		case SDLK_LEFT			: return Keys::LEFT;
		case SDLK_UP			: return Keys::UP;
		case SDLK_RIGHT			: return Keys::RIGHT;
		case SDLK_DOWN			: return Keys::DOWN;
		case SDLK_PRINT			: return Keys::SNAPSHOT;
		case SDLK_INSERT		: return Keys::INSERT;
		case SDLK_DELETE		: return Keys::DEL;
		case SDLK_LSHIFT		: return Keys::LSHIFT;
		case SDLK_RSHIFT		: return Keys::RSHIFT;
		case SDLK_LCTRL			: return Keys::LCTRL;
		case SDLK_RCTRL			: return Keys::RCTRL;
		case SDLK_LALT			: return Keys::LALT;
		case SDLK_RALT			: return Keys::RALT;
		case SDLK_0				: return Keys::N0;
		case SDLK_1				: return Keys::N1;
		case SDLK_2				: return Keys::N2;
		case SDLK_3				: return Keys::N3;
		case SDLK_4				: return Keys::N4;
		case SDLK_5				: return Keys::N5;
		case SDLK_6				: return Keys::N6;
		case SDLK_7				: return Keys::N7;
		case SDLK_8				: return Keys::N8;
		case SDLK_9				: return Keys::N9;
		case SDLK_a				: return Keys::A;
		case SDLK_b				: return Keys::B;
		case SDLK_c				: return Keys::C;
		case SDLK_d				: return Keys::D;
		case SDLK_e				: return Keys::E;
		case SDLK_f				: return Keys::F;
		case SDLK_g				: return Keys::G;
		case SDLK_h				: return Keys::H;
		case SDLK_i				: return Keys::I;
		case SDLK_j				: return Keys::J;
		case SDLK_k				: return Keys::K;
		case SDLK_l				: return Keys::L;
		case SDLK_m				: return Keys::M;
		case SDLK_n				: return Keys::N;
		case SDLK_o				: return Keys::O;
		case SDLK_p				: return Keys::P;
		case SDLK_q				: return Keys::Q;
		case SDLK_r				: return Keys::R;
		case SDLK_s				: return Keys::S;
		case SDLK_t				: return Keys::T;
		case SDLK_u				: return Keys::U;
		case SDLK_v				: return Keys::V;
		case SDLK_w				: return Keys::W;
		case SDLK_x				: return Keys::X;
		case SDLK_y				: return Keys::Y;
		case SDLK_z				: return Keys::Z;
		case SDLK_LSUPER		: return Keys::LOS;
		case SDLK_RSUPER		: return Keys::ROS;
		case SDLK_MENU			: return Keys::MENU;
		case SDLK_KP0			: return Keys::KP0;
		case SDLK_KP1			: return Keys::KP1;
		case SDLK_KP2			: return Keys::KP2;
		case SDLK_KP3			: return Keys::KP3;
		case SDLK_KP4			: return Keys::KP4;
		case SDLK_KP5			: return Keys::KP5;
		case SDLK_KP6			: return Keys::KP6;
		case SDLK_KP7			: return Keys::KP7;
		case SDLK_KP8			: return Keys::KP8;
		case SDLK_KP9			: return Keys::KP9;
		case SDLK_KP_MULTIPLY	: return Keys::MULTIPLY;
		case SDLK_KP_PLUS		: return Keys::ADD;
		case SDLK_KP_ENTER		: return Keys::RETURN;
		case SDLK_KP_MINUS		: return Keys::SUBTRACT;
		case SDLK_KP_PERIOD		: return Keys::PERIOD;
		case SDLK_KP_DIVIDE		: return Keys::DIVIDE;
		case SDLK_F1			: return Keys::F1;
		case SDLK_F2			: return Keys::F2;
		case SDLK_F3			: return Keys::F3;
		case SDLK_F4			: return Keys::F4;
		case SDLK_F5			: return Keys::F5;
		case SDLK_F6			: return Keys::F6;
		case SDLK_F7			: return Keys::F7;
		case SDLK_F8			: return Keys::F8;
		case SDLK_F9			: return Keys::F9;
		case SDLK_F10			: return Keys::F10;
		case SDLK_F11			: return Keys::F11;
		case SDLK_F12			: return Keys::F12;
		case SDLK_CAPSLOCK		: return Keys::CAPS_LOCK;
		case SDLK_NUMLOCK		: return Keys::NUM_LOCK;
		case SDLK_SCROLLOCK		: return Keys::SCROLL_LOCK;
		default					: return Keys::NONE;
	}
}
#endif

#if defined(USE_JOYSTICK) && defined(SUPPORT_JOYSTICK)
Keys::InputKey SdlJKey2InputKey(int button_index) {
	switch (button_index) {
		case 0	: return Keys::JOY_0;
		case 1	: return Keys::JOY_1;
		case 2	: return Keys::JOY_2;
		case 3	: return Keys::JOY_3;
		case 4	: return Keys::JOY_4;
		case 5	: return Keys::JOY_5;
		case 6	: return Keys::JOY_6;
		case 7	: return Keys::JOY_7;
		case 8	: return Keys::JOY_8;
		case 9	: return Keys::JOY_9;
		case 10	: return Keys::JOY_10;
		case 11	: return Keys::JOY_11;
		case 12	: return Keys::JOY_12;
		case 13	: return Keys::JOY_13;
		case 14	: return Keys::JOY_14;
		case 15	: return Keys::JOY_15;
		case 16	: return Keys::JOY_16;
		case 17	: return Keys::JOY_17;
		case 18	: return Keys::JOY_18;
		case 19	: return Keys::JOY_19;
		case 20	: return Keys::JOY_20;
		case 21	: return Keys::JOY_21;
		case 22	: return Keys::JOY_22;
		case 23	: return Keys::JOY_23;
		case 24	: return Keys::JOY_24;
		case 25	: return Keys::JOY_25;
		case 26	: return Keys::JOY_23;
		case 27	: return Keys::JOY_27;
		case 28	: return Keys::JOY_28;
		case 29	: return Keys::JOY_29;
		case 30	: return Keys::JOY_30;
		case 31	: return Keys::JOY_31;
		default : return Keys::NONE;
	}
}
#endif

int FilterUntilFocus(const SDL_Event* evnt) {
	switch (evnt->type) {
	case SDL_QUIT:
		Player::exit_flag(true);
		return 1;

	case SDL_ACTIVEEVENT:
		return evnt->active.state & SDL_APPINPUTFOCUS;

	default:
		return 0;
	}
}

#ifdef GEKKO
void GekkoResetCallback() {
	Player().reset_flag = true;
}
#endif

#endif
