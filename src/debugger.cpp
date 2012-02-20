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

/////////////////////////////////////////////////////////////////////////////
// Headers
/////////////////////////////////////////////////////////////////////////////
#include "debugger.h"
#ifdef EASYRPG_DEBUGGER
#include <wx/event.h>
#include <wx/init.h>
#include <wx/thread.h>
#include "debugger_main.h"
#include "debugger_mainframe.h"
#include "output.h"
#endif

namespace Debugger {
	bool player_suspended;
	DebuggerMainFrame* frame;
	DebuggerThread* thread;
}

void Debugger::Init() {
#ifdef EASYRPG_DEBUGGER
	player_suspended = false;
	frame = NULL;
	thread = NULL;
	wxInitializer initializer;
#endif
}

void Debugger::Open() {
#ifdef EASYRPG_DEBUGGER
	if (!frame)
	{
		Output::Debug("Debugger started");
		thread = new DebuggerThread();
	}
#endif
}

bool Debugger::IsDebuggerRunning() {
#ifdef EASYRPG_DEBUGGER
	return frame != NULL;
#else
	return false;
#endif
}


bool Debugger::IsPlayerSuspended() {
#ifdef EASYRPG_DEBUGGER
	return player_suspended;
#else
	return false;
#endif
}


void Debugger::Shutdown() {
#ifdef EASYRPG_DEBUGGER
	if (frame) {
		Debugger::SendEventToDebugger(Debugger::DebuggerCode_close);
		thread->Wait();
		frame = NULL;
		Output::Debug("Debugger closed");
	}
#endif
}

void Debugger::SendEventToPlayer(Debugger::PlayerCode code, int data, int data2) {
#ifdef EASYRPG_DEBUGGER
	SDL_Event dbgEvent;
	dbgEvent.type = SDL_USEREVENT_EASYRPG_DEBUGGER;
	dbgEvent.user.code = code;
	dbgEvent.user.data1 = (void*)data;
	dbgEvent.user.data2 = (void*)data2;
	SDL_PushEvent(&dbgEvent);
#else
	(void)code; (void)data; (void)data2;
#endif
}

void Debugger::SendEventToDebugger(DebuggerCode code, int data, int data2) {
#ifdef EASYRPG_DEBUGGER
	wxCommandEvent event(DebuggerMainFrame::PlayerEvent, code);
	event.SetInt(data);
	event.SetExtraLong(data2);
	wxPostEvent(Debugger::frame, event);
#else
	(void)code; (void)data; (void)data2;
#endif
}
