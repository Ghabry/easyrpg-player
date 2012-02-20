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

////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////
#include "SDL_events.h"

////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////
#define SDL_USEREVENT_EASYRPG_DEBUGGER (SDL_USEREVENT + 1)

////////////////////////////////////////////////////////////
/// Debugger Data namespace
////////////////////////////////////////////////////////////
class DebuggerMainFrame;
class DebuggerThread;

namespace Debugger {
	/// Event codes sent to the debugger
	enum DebuggerCode {
		DebuggerCode_suspended = 0,
		DebuggerCode_close,
		DebuggerCode_switch_changed,
		DebuggerCode_variable_changed
	};

	/// Event codes sent to the player
	enum PlayerCode {
		PlayerCode_suspend = 0,
		PlayerCode_continue,
		PlayerCode_terminate
	};

	/// If the player was suspended by the debugger
	extern bool player_suspended;
	/// Handle to the Frame displayed by the debugger
	extern DebuggerMainFrame* frame;
	/// Handle to the thread running the 
	extern DebuggerThread* thread;

	void Init();

	void Open();

	void Shutdown();

	/// @return If the debug window is open
	bool IsDebuggerRunning();

	/// @return If the player was suspended by the debugger
	bool IsPlayerSuspended();

	void SendEventToPlayer(PlayerCode code, int data = -1, int data2 = -1);

	void SendEventToDebugger(DebuggerCode code, int data = -1, int data2 = -1);

}

#endif
