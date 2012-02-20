///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
#ifdef EASYRPG_DEBUGGER
#include "debugger_mainpanel_gui.h"

///////////////////////////////////////////////////////////////////////////

DebuggerMainPanelGui::DebuggerMainPanelGui( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );
	
	m_panel1 = new wxPanel( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxFlexGridSizer* fgSizer1;
	fgSizer1 = new wxFlexGridSizer( 1, 2, 0, 0 );
	fgSizer1->AddGrowableCol( 0 );
	fgSizer1->AddGrowableCol( 1 );
	fgSizer1->AddGrowableRow( 0 );
	fgSizer1->SetFlexibleDirection( wxBOTH );
	fgSizer1->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	wxArrayString switches_listChoices;
	switches_list = new wxCheckListBox( m_panel1, wxID_ANY, wxDefaultPosition, wxDefaultSize, switches_listChoices, 0 );
	fgSizer1->Add( switches_list, 0, wxALL|wxEXPAND, 5 );
	
	variables_list = new wxListBox( m_panel1, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	fgSizer1->Add( variables_list, 0, wxALL|wxEXPAND, 5 );
	
	m_panel1->SetSizer( fgSizer1 );
	m_panel1->Layout();
	fgSizer1->Fit( m_panel1 );
	bSizer3->Add( m_panel1, 1, wxEXPAND | wxALL, 5 );
	
	this->SetSizer( bSizer3 );
	this->Layout();
}

DebuggerMainPanelGui::~DebuggerMainPanelGui()
{
}
#endif
