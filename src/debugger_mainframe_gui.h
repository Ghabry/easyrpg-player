///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
#ifdef EASYRPG_DEBUGGER
#ifndef __debugger_mainframe_gui__
#define __debugger_mainframe_gui__

#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/gdicmn.h>
#include <wx/toolbar.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include "debugger_mainpanel.h"
#include <wx/sizer.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////

#define DEBUGGER_TOOL_RUN 1000
#define DEBUGGER_TOOL_PAUSE 1001

///////////////////////////////////////////////////////////////////////////////
/// Class Debugger_MainFrameGui
///////////////////////////////////////////////////////////////////////////////
class Debugger_MainFrameGui : public wxFrame 
{
	private:
	
	protected:
		wxToolBar* toolbar;
		Debugger_MainPanel* panel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnFrameClose( wxCloseEvent& event ) { event.Skip(); }
		virtual void OnRunToolClicked( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnPauseToolClicked( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		Debugger_MainFrameGui( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("EasyRPG Debugger"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 500,300 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );
		~Debugger_MainFrameGui();
	
};

#endif //__debugger_mainframe_gui__
#endif
