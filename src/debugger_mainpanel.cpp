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
#include "debugger_mainpanel.h"
#include "debugger_mainframe.h"
#include "debugger.h"
#include "player.h"
#include "main_data.h"

/////////////////////////////////////////////////////////////////////////////
BEGIN_EVENT_TABLE(Debugger_MainPanel, wxPanel)
	EVT_COMMAND(Debugger::DebuggerCode_suspended, Debugger_MainFrame::PlayerEvent, Debugger_MainPanel::OnPlayerSuspendedEvent)
END_EVENT_TABLE()

/////////////////////////////////////////////////////////////////////////////
Debugger_MainPanel::Debugger_MainPanel(wxWindow* parent) :
	Debugger_MainPanelGui(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL),
	first_suspend(true) {
}

/////////////////////////////////////////////////////////////////////////////
void Debugger_MainPanel::OnPlayerSuspendedEvent(wxCommandEvent& event) {
	std::vector<bool>& switches = Main_Data::game_data.system.switches;
	size_t i = 0;

	if (first_suspend) {
		first_suspend = false;

		size_t i = 0;
		for (std::vector<bool>::iterator it = switches.begin(); it != switches.end(); ++it) {
			switches_list->Append(
				wxString::Format(wxT("%04d: %s"), i+1, wxString::FromUTF8(Data::switches[i].name.c_str())));
			if (*it) {
				switches_list->Check(i, true);
			}
			++i;
		}
	} else {
		for (std::vector<bool>::iterator it = switches.begin(); it != switches.end(); ++it) {
			if (switches_list->IsChecked(i) != *it) {
				switches_list->Check(i, *it);
			}
			++i;
		}
	}

	variables_list->Clear();
	std::vector<uint32>& variables = Main_Data::game_data.system.variables;
	i = 0;
	for (std::vector<uint32>::iterator it = variables.begin(); it != variables.end(); ++it) {
		variables_list->Append(
			wxString::Format(wxT("%04d: %s (%d)"), i+1, wxString::FromUTF8(Data::variables[i].name.c_str()), *it));
		++i;
	}
}

#endif
