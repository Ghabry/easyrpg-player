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
#ifndef _EASYRPG_DEBUGGER_MAINFRAME_H_
#define _EASYRPG_DEBUGGER_MAINFRAME_H_

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <wx/wx.h>
#include "debugger_mainframe_gui.h"
#include "debugger_mainpanel.h"

class DebuggerMainFrame : public DebuggerMainFrameGui {

public:
	DebuggerMainFrame();

	static const wxEventType PlayerEvent;

private:

	void OnRunToolClicked(wxCommandEvent& event);
	void OnPauseToolClicked(wxCommandEvent& event);
	void OnFrameClose(wxCloseEvent& event);

	void OnPlayerSuspendedEvent(wxCommandEvent& event);
	void OnPlayerCloseEvent(wxCommandEvent& event);
	void OnOtherPlayerEvent(wxCommandEvent& event);

	DECLARE_EVENT_TABLE();
};

#endif
#endif
