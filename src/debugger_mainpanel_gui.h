///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
#ifdef EASYRPG_DEBUGGER
#ifndef __debugger_mainpanel_gui__
#define __debugger_mainpanel_gui__

#include <wx/string.h>
#include <wx/checklst.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/panel.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class Debugger_MainPanelGui
///////////////////////////////////////////////////////////////////////////////
class Debugger_MainPanelGui : public wxPanel 
{
	private:
	
	protected:
		wxPanel* m_panel1;
		wxCheckListBox* switches_list;
		wxListBox* variables_list;
	
	public:
		
		Debugger_MainPanelGui( wxWindow* parent, wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 500,300 ), long style = wxTAB_TRAVERSAL );
		~Debugger_MainPanelGui();
	
};

#endif //__debugger_mainpanel_gui__
#endif
