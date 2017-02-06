#include "pch.h"
#include "DxAppXAMLMain.h"
#include "DirectXHelper.h"

#include "bitmap.h"
#include "player.h"
#include "graphics.h"
#include "input.h"

#include "uwp_ui.h"

using namespace DxAppXAML;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// Lädt und initialisiert die Anwendungsobjekte, wenn die Anwendung geladen wird.
DxAppXAMLMain::DxAppXAMLMain(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources), m_pointerLocationX(0.0f)
{
	// Registrieren, um über Geräteverlust oder Neuerstellung benachrichtigt zu werden
	m_deviceResources->RegisterDeviceNotify(this);

	// TODO: Dies durch Ihre App-Inhaltsinitialisierung ersetzen.
	D2D1_BITMAP_PROPERTIES1 properties;
	properties.dpiX = 96;
	properties.dpiY = 96;
	properties.pixelFormat = { DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_IGNORE };
	properties.colorContext = 0;
	properties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET;

	deviceResources->GetD2DDeviceContext()->CreateBitmap(m_sceneBitmapSize, nullptr, 0,
		properties,
		&m_sceneBitmap);

	DX::ThrowIfFailed(
		m_deviceResources->GetD2DFactory()->CreateDrawingStateBlock(&m_stateBlock)
	);

	// PLAYER INIT HIER
	DisplayUi.reset(new UwpUi(320, 240));
	Player::Init(0, nullptr);
	Graphics::Init();
	Input::Init();
}

DxAppXAMLMain::~DxAppXAMLMain()
{
	// Registrierung der Gerätebenachrichtigung aufheben
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Aktualisiert den Anwendungszustand, wenn sich die Fenstergröße ändert (z. B. Änderung der Geräteausrichtung)
void DxAppXAMLMain::CreateWindowSizeDependentResources() 
{
	// TODO: Dies durch Ihre größenabhängige Initialisierung Ihres App-Inhalts ersetzen.
}

void DxAppXAMLMain::StartRenderLoop()
{
	// Wenn der Animations-Renderloop bereits ausgeführt wird, keinen weiteren Thread starten.
	if (m_renderLoopWorker != nullptr && m_renderLoopWorker->Status == AsyncStatus::Started)
	{
		return;
	}

	if (m_updateLoopWorker != nullptr && m_updateLoopWorker->Status == AsyncStatus::Started) {
		return;
	}

	// Aufgabe erstellen, die für einen Hintergrundthread ausgeführt wird.
	auto workItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action)
	{
		// Den aktualisierten Frame berechnen und einmal pro VBI rendern.
		while (action->Status == AsyncStatus::Started)
		{
			critical_section::scoped_lock lock(m_criticalSection);
			
			if (Render())
			{
				m_deviceResources->Present();
			}
		}
	});

	// Aufgabe in einem Hintergrundthread mit dezidiert hoher Priorität ausführen.
	m_renderLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

	// Aufgabe erstellen, die für einen Hintergrundthread ausgeführt wird.
	auto updateItemHandler = ref new WorkItemHandler([this](IAsyncAction ^ action) {
		// Will not return before exit
		Player::Run();
	});

	// Aufgabe in einem Hintergrundthread mit dezidiert hoher Priorität ausführen.
	m_updateLoopWorker = ThreadPool::RunAsync(updateItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);
}

void DxAppXAMLMain::StopRenderLoop()
{
	Player::exit_flag = true;
	m_renderLoopWorker->Cancel();
	// Bad, need sync
	m_updateLoopWorker->Cancel();
}

// Aktualisiert den Anwendungszustand ein Mal pro Frame.
void DxAppXAMLMain::Update() 
{
	ProcessInput();

	// Die Szeneobjekte aktualisieren.
}

// Alle Benutzereingaben vor dem Aktualisieren des Spielzustands verarbeiten
void DxAppXAMLMain::ProcessInput()
{
	// TODO: Hier Eingabebehandlung pro Frame hinzufügen.
}

// Rendert den aktuellen Frame dem aktuellen Anwendungszustand entsprechend.
// Gibt True zurück, wenn der Frame gerendert wurde und angezeigt werden kann.
bool DxAppXAMLMain::Render() 
{
	auto context_3d = m_deviceResources->GetD3DDeviceContext();

	// Den Viewport zurücksetzen, damit der gesamte Bildschirm das Ziel ist.
	auto viewport = m_deviceResources->GetScreenViewport();
	context_3d->RSSetViewports(1, &viewport);

	// Renderziele zum Bildschirm zurücksetzen.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context_3d->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// Den Hintergrundpuffer und die Ansicht der Tiefenschablone bereinigen.
	context_3d->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::CornflowerBlue);
	context_3d->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Die Szeneobjekte rendern.
	// TODO: Dies mit den Inhaltsrenderingfunktionen Ihrer App ersetzen.
	ID2D1DeviceContext* context_2d = m_deviceResources->GetD2DDeviceContext();
	Windows::Foundation::Size logicalSize = m_deviceResources->GetLogicalSize();

	// GET BITMAP BUFFER
	if (!DisplayUi) {
		return false;
	}

	BitmapRef bitmap = DisplayUi->GetDisplaySurface();

	context_2d->SaveDrawingState(m_stateBlock.Get());
	context_2d->BeginDraw();

	// Position in der unteren rechten Ecke

	D2D1_RECT_U r;
	r.top = 0;
	r.left = 0;
	r.right = m_sceneBitmapSize.width;
	r.bottom = m_sceneBitmapSize.height;

	m_sceneBitmap->CopyFromMemory(&r, (*bitmap).pixels(), (*bitmap).pitch());

	Windows::Foundation::Size outputSize = m_deviceResources->GetOutputSize();
	float aspectRatio = outputSize.Width / outputSize.Height;

	D2D1_RECT_F f;
	f.top = 0;
	f.left = 0;
	f.right = outputSize.Width;
	f.bottom = outputSize.Height;

	context_2d->SetTransform(D2D1::Matrix3x2F::Translation(0, 0) * m_deviceResources->GetOrientationTransform2D());

	context_2d->DrawBitmap(m_sceneBitmap.Get(), f, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);

	// D2DERR_RECREATE_TARGET hier ignorieren. Dieser Fehler weist darauf hin, dass das Gerät
	// nicht mehr vorhanden ist. Es wird beim nächsten Aufruf von "Present" behandelt.
	HRESULT hr = context_2d->EndDraw();
	if (hr != D2DERR_RECREATE_TARGET) {
		DX::ThrowIfFailed(hr);
	}

	context_2d->RestoreDrawingState(m_stateBlock.Get());

	return true;
}

// Weist Renderer darauf hin, dass die Geräteressourcen freigegeben werden müssen.
void DxAppXAMLMain::OnDeviceLost()
{
}

// Weist Renderer darauf hin, dass die Geräteressourcen jetzt erstellt werden können.
void DxAppXAMLMain::OnDeviceRestored()
{
	CreateWindowSizeDependentResources();
}
