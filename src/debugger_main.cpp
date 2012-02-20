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

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "debugger_main.h"
#include "debugger_mainframe.h"
#include "debugger.h"

IMPLEMENT_APP_NO_MAIN(DebuggerApp)

bool DebuggerApp::OnInit() {
	Debugger::frame = new DebuggerMainFrame();
	Debugger::frame->Show();
	return true;
}

DebuggerThread::DebuggerThread() : wxThread(wxTHREAD_JOINABLE) {
	if(wxTHREAD_NO_ERROR == Create()) {
		Run();
	}
}

void* DebuggerThread::Entry() {
	wxEntry(0, NULL);
	return static_cast<ExitCode>(NULL);
}

#endif
