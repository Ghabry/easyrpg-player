///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Sep  8 2010)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////
#ifdef EASYRPG_DEBUGGER
#include "debugger_mainframe_gui.h"

///////////////////////////////////////////////////////////////////////////

Debugger_MainFrameGui::Debugger_MainFrameGui( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	
	toolbar = this->CreateToolBar( wxTB_HORIZONTAL|wxTB_NOICONS|wxTB_TEXT, wxID_ANY ); 
	toolbar->AddTool( DEBUGGER_TOOL_RUN, wxT("Run"), wxNullBitmap, wxNullBitmap, wxITEM_RADIO, wxEmptyString, wxEmptyString ); 
	toolbar->AddTool( DEBUGGER_TOOL_PAUSE, wxT("Pause"), wxNullBitmap, wxNullBitmap, wxITEM_RADIO, wxEmptyString, wxEmptyString ); 
	toolbar->Realize();
	
	wxBoxSizer* bSizer1;
	bSizer1 = new wxBoxSizer( wxVERTICAL );
	
	wxFlexGridSizer* fgSizer1;
	fgSizer1 = new wxFlexGridSizer( 1, 1, 0, 0 );
	fgSizer1->AddGrowableCol( 0 );
	fgSizer1->AddGrowableRow( 0 );
	fgSizer1->SetFlexibleDirection( wxBOTH );
	fgSizer1->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
	
	panel = new Debugger_MainPanel(this);
	fgSizer1->Add( panel, 0, wxALL|wxEXPAND, 0 );
	
	bSizer1->Add( fgSizer1, 1, wxEXPAND, 5 );
	
	this->SetSizer( bSizer1 );
	this->Layout();
	
	this->Centre( wxBOTH );
	
	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( Debugger_MainFrameGui::OnFrameClose ) );
	this->Connect( DEBUGGER_TOOL_RUN, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( Debugger_MainFrameGui::OnRunToolClicked ) );
	this->Connect( DEBUGGER_TOOL_PAUSE, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( Debugger_MainFrameGui::OnPauseToolClicked ) );
}

Debugger_MainFrameGui::~Debugger_MainFrameGui()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( Debugger_MainFrameGui::OnFrameClose ) );
	this->Disconnect( DEBUGGER_TOOL_RUN, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( Debugger_MainFrameGui::OnRunToolClicked ) );
	this->Disconnect( DEBUGGER_TOOL_PAUSE, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( Debugger_MainFrameGui::OnPauseToolClicked ) );
	
}
#endif
