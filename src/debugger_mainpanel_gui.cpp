///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
#ifdef EASYRPG_DEBUGGER
#include "debugger_mainpanel_gui.h"

///////////////////////////////////////////////////////////////////////////

Debugger_MainPanelGui::Debugger_MainPanelGui( wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style ) : wxPanel( parent, id, pos, size, style )
{
	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );
	
	wxFlexGridSizer* fgSizer1;
	fgSizer1 = new wxFlexGridSizer( 1, 2, 0, 0 );
	fgSizer1->AddGrowableCol( 0 );
	fgSizer1->AddGrowableCol( 1 );
	fgSizer1->AddGrowableRow( 0 );
	fgSizer1->SetFlexibleDirection( wxBOTH );
	fgSizer1->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	wxArrayString switches_listChoices;
	switches_list = new wxCheckListBox( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, switches_listChoices, 0 );
	fgSizer1->Add( switches_list, 0, wxALL|wxEXPAND, 5 );
	
	variables_list = new wxListBox( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, NULL, 0 ); 
	fgSizer1->Add( variables_list, 0, wxALL|wxEXPAND, 5 );
	
	bSizer3->Add( fgSizer1, 1, wxEXPAND, 5 );
	
	this->SetSizer( bSizer3 );
	this->Layout();
	
	// Connect Events
	switches_list->Connect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( Debugger_MainPanelGui::OnSwitchesListToggled ), NULL, this );
}

Debugger_MainPanelGui::~Debugger_MainPanelGui()
{
	// Disconnect Events
	switches_list->Disconnect( wxEVT_COMMAND_CHECKLISTBOX_TOGGLED, wxCommandEventHandler( Debugger_MainPanelGui::OnSwitchesListToggled ), NULL, this );
	
}
#endif
