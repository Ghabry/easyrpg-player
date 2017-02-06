//
// DirectXPage.xaml.cpp
// Implementierung der DirectXPage-Klasse
//

#include "pch.h"

#include "DirectXPage.xaml.h"

using namespace DxAppXAML;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;

DirectXPage::DirectXPage():
	m_windowVisible(true),
	m_coreInput(nullptr)
{
	InitializeComponent();

	// Ereignishandler für Seitenlebenszyklus registrieren.
	CoreWindow^ window = Window::Current->CoreWindow;

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &DirectXPage::OnVisibilityChanged);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDisplayContentsInvalidated);

	swapChainPanel->CompositionScaleChanged += 
		ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &DirectXPage::OnCompositionScaleChanged);

	swapChainPanel->SizeChanged +=
		ref new SizeChangedEventHandler(this, &DirectXPage::OnSwapChainPanelSizeChanged);

	// Zu diesem Zeitpunkt haben wir Zugriff auf das Gerät. 
	// Wir können die geräteabhängigen Ressourcen erstellen.
	m_deviceResources = std::make_shared<DX::DeviceResources>();
	m_deviceResources->SetSwapChainPanel(swapChainPanel);

	// Unseren SwapChainPanel registrieren, um Zeigerereignisse für die unabhängige Eingabe zu erhalten
	auto workItemHandler = ref new WorkItemHandler([this] (IAsyncAction ^)
	{
		// Die CoreIndependentInputSource löst auf dem Thread Zeigerereignisse für die angegebenen Gerätetypen aus, auf dem sie erstellt wurde.
		m_coreInput = swapChainPanel->CreateCoreIndependentInputSource(
			Windows::UI::Core::CoreInputDeviceTypes::Mouse |
			Windows::UI::Core::CoreInputDeviceTypes::Touch |
			Windows::UI::Core::CoreInputDeviceTypes::Pen
			);

		// Für Zeigerereignisse registrieren, die für den Hintergrundthread ausgelöst werden.
		m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerPressed);
		m_coreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerMoved);
		m_coreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerReleased);

		// Eingehende Eingabemeldungen sofort verarbeiten.
		m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
	});

	// Aufgabe in einem Hintergrundthread mit dezidiert hoher Priorität ausführen.
	m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

	m_main = std::unique_ptr<DxAppXAMLMain>(new DxAppXAMLMain(m_deviceResources));
	m_main->StartRenderLoop();
}

DirectXPage::~DirectXPage()
{
	// Bei Zerstörung das Rendering und Verarbeiten von Ereignissen beenden.
	m_main->StopRenderLoop();
	m_coreInput->Dispatcher->StopProcessEvents();
}

// Speichert den aktuellen Zustand der App für angehaltene und beendete Ereignisse.
void DirectXPage::SaveInternalState(IPropertySet^ state)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->Trim();

	// Rendering beenden, wenn die App angehalten wird.
	m_main->StopRenderLoop();

	// Code zum Speichern des App-Zustands hier einfügen.
}

// Lädt den aktuellen Zustand der App für Fortsetzungsereignisse.
void DirectXPage::LoadInternalState(IPropertySet^ state)
{
	// Code zum Laden des App-Zustands hier einfügen.

	// Rendering starten, wenn die App fortgesetzt wird.
	m_main->StartRenderLoop();
}

// Ereignishandler für Fenster.

void DirectXPage::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
	if (m_windowVisible)
	{
		m_main->StartRenderLoop();
	}
	else
	{
		m_main->StopRenderLoop();
	}
}

// DisplayInformation-Ereignishandler.

void DirectXPage::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	// Hinweis: Der hier abgerufene Wert für "LogicalDpi" stimmt ggf. nicht mit dem effektiven DPI-Wert der App überein,
	// wenn die Skalierung für Geräte mit hoher Auflösung erfolgt. Sobald der DPI-Wert für "DeviceResources" festgelegt wurde,
	// sollten Sie ihn immer mithilfe der Methode "GetDpi" abrufen.
	// Weitere Informationen finden Sie unter "DeviceResources.cpp".
	m_deviceResources->SetDpi(sender->LogicalDpi);
	m_main->CreateWindowSizeDependentResources();
}

void DirectXPage::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->CreateWindowSizeDependentResources();
}

void DirectXPage::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->ValidateDevice();
}

// Aufruf erfolgt, wenn die App-Leisten-Schaltfläche gedrückt wird.
void DirectXPage::AppBarButton_Click(Object^ sender, RoutedEventArgs^ e)
{
	// Die App-Leiste verwenden, wenn es für Ihre App passt. Die App-Leiste entwerfen, 
	// dann Ereignishandler (wie diesen) eingeben.
}

void DirectXPage::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{
	// Nachverfolgung der Zeigerbewegung starten, wenn der Zeiger gedrückt wird.
	m_main->StartTracking();
}

void DirectXPage::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{
	// Den Zeigernachverfolgungscode aktualisieren.
	if (m_main->IsTracking())
	{
		m_main->TrackingUpdate(e->CurrentPoint->Position.X);
	}
}

void DirectXPage::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{
	// Nachverfolgung der Zeigerbewegung beenden, wenn der Zeiger freigegeben wird.
	m_main->StopTracking();
}

void DirectXPage::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetCompositionScale(sender->CompositionScaleX, sender->CompositionScaleY);
	m_main->CreateWindowSizeDependentResources();
}

void DirectXPage::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
	critical_section::scoped_lock lock(m_main->GetCriticalSection());
	m_deviceResources->SetLogicalSize(e->NewSize);
	m_main->CreateWindowSizeDependentResources();
}
