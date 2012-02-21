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
#ifndef _EASYRPG_DEBUGGER_MAINPANEL_H_
#define _EASYRPG_DEBUGGER_MAINPANEL_H_

/////////////////////////////////////////////////////////////////////////////
// Headers
/////////////////////////////////////////////////////////////////////////////
#include <wx/event.h>
#include <wx/window.h>
#include "debugger_mainpanel_gui.h"

/////////////////////////////////////////////////////////////////////////////
/// Debugger_MainPanel class.
/// This is the panel displayed in the main area of the MainFrame.
/////////////////////////////////////////////////////////////////////////////
class Debugger_MainPanel : public Debugger_MainPanelGui {

public:
	Debugger_MainPanel(wxWindow* parent);

	/////////////////////////////////////////////////////////////////////////
	/// Called when the player continues execution.
	/////////////////////////////////////////////////////////////////////////
	void PlayerContinued();

private:
	/// Whether its the first time the player was suspended
	bool first_suspend;

	/////////////////////////////////////////////////////////////////////////
	/// One of the checkboxes in the switches list was toggled. Change the
	/// switch state in the player.
	/////////////////////////////////////////////////////////////////////////
	void OnSwitchesListToggled(wxCommandEvent& event);

	/////////////////////////////////////////////////////////////////////////
	/// Received when the player got suspended.
	/////////////////////////////////////////////////////////////////////////
	void OnPlayerSuspendedEvent(wxCommandEvent& event);
	
	DECLARE_EVENT_TABLE();
};

#endif
#endif
