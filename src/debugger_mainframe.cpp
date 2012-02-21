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

#ifdef EASYRPG_DEBUGGER

/////////////////////////////////////////////////////////////////////////////
// Headers
/////////////////////////////////////////////////////////////////////////////
#include "debugger_mainframe.h"
#include "debugger.h"
#include "player.h"

/////////////////////////////////////////////////////////////////////////////
const wxEventType Debugger_MainFrame::PlayerEvent = wxNewEventType();

BEGIN_EVENT_TABLE(Debugger_MainFrame, wxFrame)
	EVT_COMMAND(Debugger::DebuggerCode_suspended, Debugger_MainFrame::PlayerEvent, Debugger_MainFrame::OnPlayerSuspendedEvent)
	EVT_COMMAND(Debugger::DebuggerCode_close, Debugger_MainFrame::PlayerEvent, Debugger_MainFrame::OnPlayerCloseEvent)
	EVT_COMMAND(wxID_ANY, Debugger_MainFrame::PlayerEvent, Debugger_MainFrame::OnOtherPlayerEvent)
END_EVENT_TABLE()

/////////////////////////////////////////////////////////////////////////////
Debugger_MainFrame::Debugger_MainFrame() : Debugger_MainFrameGui(0) {
}

/////////////////////////////////////////////////////////////////////////////
// Gui events
void Debugger_MainFrame::OnRunToolClicked(wxCommandEvent& event) {
	if (toolbar->GetToolState(DEBUGGER_TOOL_RUN)) {
		Debugger::player_suspended = false;
		Debugger::SendEventToPlayer(Debugger::PlayerCode_continue);
	}
}

void Debugger_MainFrame::OnPauseToolClicked(wxCommandEvent& event) {
	if (toolbar->GetToolState(DEBUGGER_TOOL_PAUSE)) {
		this->Enable(false);
		Debugger::SendEventToPlayer(Debugger::PlayerCode_suspend);
	}
}

void Debugger_MainFrame::OnFrameClose(wxCloseEvent& event) {
	if (!event.CanVeto()) {
		Debugger::SendEventToPlayer(Debugger::PlayerCode_continue);
		Destroy();
	} else {
		event.Veto();
		Debugger::SendEventToPlayer(Debugger::PlayerCode_continue);
		Debugger::SendEventToPlayer(Debugger::PlayerCode_terminate);
	}
}


/////////////////////////////////////////////////////////////////////////////
// Player events
void Debugger_MainFrame::OnPlayerSuspendedEvent(wxCommandEvent& event) {
	this->Enable(true);
	Debugger::player_suspended = true;
	OnOtherPlayerEvent(event);
}

void Debugger_MainFrame::OnPlayerCloseEvent(wxCommandEvent& event) {
	if (event.GetId() == Debugger::DebuggerCode_close) {
		Close(true);
	}
}

void Debugger_MainFrame::OnOtherPlayerEvent(wxCommandEvent& event) {
	panel->HandleWindowEvent(event);
}



#endif
