//
// App.xaml.cpp
// Implementierung der App-Klasse
//

#include "pch.h"

#include "DirectXPage.xaml.h"

using namespace DxAppXAML;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

/// <summary>
/// Initialisiert das Singletonanwendungsobjekt. Dies ist die erste Zeile von erstelltem Code
/// und daher das logische Äquivalent von main() bzw. WinMain().
/// </summary>
App::App()
{
	InitializeComponent();
	Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
	Resuming += ref new EventHandler<Object^>(this, &App::OnResuming);
}

/// <summary>
/// Wird aufgerufen, wenn die Anwendung durch den Endbenutzer normal gestartet wird. Weitere Einstiegspunkte
/// werden verwendet, wenn die Anwendung zum Öffnen einer bestimmten Datei, zum Anzeigen
/// von Suchergebnissen usw. gestartet wird.
/// </summary>
/// <param name="e">Details über Startanforderung und -prozess.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{
#if _DEBUG
	if (IsDebuggerPresent())
	{
		DebugSettings->EnableFrameRateCounter = true;
	}
#endif

	if (m_directXPage == nullptr)
	{
		m_directXPage = ref new DirectXPage();
	}

	if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
	{
		m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
	}

	// Platzieren Sie die Seite im aktuellen Fenster, und stellen Sie sicher, dass es aktiv ist.
	Window::Current->Content = m_directXPage;
	Window::Current->Activate();
}

/// <summary>
/// Wird aufgerufen, wenn die Ausführung der Anwendung angehalten wird.  Der Anwendungszustand wird gespeichert,
/// ohne zu wissen, ob die Anwendung beendet oder fortgesetzt wird und die Speicherinhalte dabei
/// unbeschädigt bleiben.
/// </summary>
/// <param name="sender">Die Quelle der Anhalteanforderung.</param>
/// <param name="e">Details zur Anhalteanforderung.</param>
void App::OnSuspending(Object^ sender, SuspendingEventArgs^ e)
{
	(void) sender;	// Nicht verwendeter Parameter
	(void) e;	// Nicht verwendeter Parameter

	m_directXPage->SaveInternalState(ApplicationData::Current->LocalSettings->Values);
}

/// <summary>
/// Wird aufgerufen, wenn die Ausführung der Anwendung fortgesetzt wird.
/// </summary>
/// <param name="sender">Die Quelle der Fortsetzungsanforderung.</param>
/// <param name="args">Details zur Fortsetzungsanforderung.</param>
void App::OnResuming(Object ^sender, Object ^args)
{
	(void) sender; // Nicht verwendeter Parameter
	(void) args; // Nicht verwendeter Parameter

	m_directXPage->LoadInternalState(ApplicationData::Current->LocalSettings->Values);
}
