/////////////////////////////////////////////////////////////////////////////
// This file is part of EasyRPG Player.
//
// EasyRPG Player is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyRPG Player is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
/////////////////////////////////////////////////////////////////////////////

#ifndef _EASYRPG_DEBUGGER_DATA_H_
#define _EASYRPG_DEBUGGER_DATA_H_

/////////////////////////////////////////////////////////////////////////////
// Includes
/////////////////////////////////////////////////////////////////////////////
#include "SDL_events.h"

/////////////////////////////////////////////////////////////////////////////
// Defines
/////////////////////////////////////////////////////////////////////////////
#define SDL_USEREVENT_EASYRPG_DEBUGGER (SDL_USEREVENT + 1)

/////////////////////////////////////////////////////////////////////////////
/// Debugger namespace
/////////////////////////////////////////////////////////////////////////////
class Debugger_MainFrame;
class Debugger_Thread;

namespace Debugger {
	/// Event codes sent to the debugger
	enum DebuggerCode {
		/// Player was suspended
		DebuggerCode_suspended = 0,
		/// Player is closing, shutdown the debugger
		DebuggerCode_close,
		/// A switch changed its state (unused)
		DebuggerCode_switch_changed,
		/// A variable changed its value (unused)
		DebuggerCode_variable_changed
	};

	/// Event codes sent to the player
	enum PlayerCode {
		/// Player shall suspend
		PlayerCode_suspend = 0,
		/// Player can continue execution
		PlayerCode_continue,
		/// Debugger wants to shutdown
		PlayerCode_terminate
	};

	/// If the player was suspended by the debugger
	extern bool player_suspended;
	/// Handle to the Frame displayed by the debugger
	extern Debugger_MainFrame* frame;
	/// Handle to the thread running the 
	extern Debugger_Thread* thread;

	/////////////////////////////////////////////////////////////////////////
	/// Initializes all datastructures and wxWidgets.
	/////////////////////////////////////////////////////////////////////////
	void Init();

	/////////////////////////////////////////////////////////////////////////
	/// Creates a Debugger_Thread which opens the debugger frame.
	/////////////////////////////////////////////////////////////////////////
	void Open();

	/////////////////////////////////////////////////////////////////////////
	/// Terminates the debugging thread and closes the debugger
	/////////////////////////////////////////////////////////////////////////
	void Shutdown();

	/////////////////////////////////////////////////////////////////////////
	/// Checks if the debug window is currently opened.
	/// @return If the debug window is open
	/////////////////////////////////////////////////////////////////////////
	bool IsDebuggerRunning();

	/////////////////////////////////////////////////////////////////////////
	/// Checks if the player is currently suspended by the debugger
	/// @return If the player is suspended by the debugger
	/////////////////////////////////////////////////////////////////////////
	bool IsPlayerSuspended();

	/////////////////////////////////////////////////////////////////////////
	/// Sends an event of type SDL_USEREVENT_EASYRPG_DEBUGGER to the SDL-
	/// event queue of the player.
	/// @param code Code sent to the player
	/// @param data Custom data
	/// @param data2 More custom data
	/////////////////////////////////////////////////////////////////////////
	void SendEventToPlayer(PlayerCode code, int data = -1, int data2 = -1);

	/////////////////////////////////////////////////////////////////////////
	/// Sends an event of type PlayerEvent to the wxWidgets-event queue of
	/// the debugger.
	/// @param code Special code sent to the debugger
	/// @param data Custom data
	/// @param data2 More custom data
	/////////////////////////////////////////////////////////////////////////
	void SendEventToDebugger(DebuggerCode code, int data = -1, int data2 = -1);
}

#endif
