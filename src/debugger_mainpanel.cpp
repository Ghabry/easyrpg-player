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
#include "game_switches.h"

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
	if (first_suspend) {
		first_suspend = false;
	}
	/*m_button1->Enable(true);
	m_button1->SetLabel(wxT("continue"));

	m_checkList1->Clear();
	for (size_t i = 0; i < Main_Data::game_data.system.switches.size(); ++i) {
		m_checkList1->Append(wxString::Format(wxT("%04d: %s"), i+1, wxString::FromUTF8(Data::switches[i].name.c_str())));
		if (Main_Data::game_data.system.switches[i]) {
			m_checkList1->Check(i, true);
		}
	}*/
 

	///Game_Switches::isValidSwitch()

	//m_checkList1->Append
}

#endif
