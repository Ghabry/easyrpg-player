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
#ifndef _EASYRPG_DEBUGGER_H_
#define _EASYRPG_DEBUGGER_H_

/////////////////////////////////////////////////////////////////////////////
// Headers
/////////////////////////////////////////////////////////////////////////////
#include <wx/app.h>
#include <wx/thread.h>

/////////////////////////////////////////////////////////////////////////////
/// Debugger_App class.
/// Implementation of the wxWidgets application.
/////////////////////////////////////////////////////////////////////////////
class Debugger_App : public wxApp {

public:
	virtual bool OnInit();
};

/////////////////////////////////////////////////////////////////////////////
/// Debugger_Thread class.
/// Implementation of the wxWidgets application.
/////////////////////////////////////////////////////////////////////////////
class Debugger_Thread : public wxThread {

public:
	Debugger_Thread();

protected:
	virtual ExitCode Entry();
};

#endif
#endif
