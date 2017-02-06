#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"
#include <windows.ui.xaml.media.dxinterop.h>

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// Auf Anzeigegeräten mit hoher Auflösung sind für das Rendern ggf. ein schneller Grafikprozessor und viel Akkuleistung erforderlich.
	// Hochauflösende Telefone können z. B. eine schlechte Akkubetriebsdauer aufweisen, wenn
	// für Spiele das Rendern mit 60 BpS und in voller Qualität versucht wird.
	// Die Entscheidung für das Rendern in voller Qualität auf allen Plattformen und für alle Formfaktoren
	// sollte wohlüberlegt sein.
	static const bool SupportHighResolutions = false;

	// Die Standardschwellenwerte, die eine "hohe Auflösung" für die Anzeige definieren. Wenn die Schwellenwerte
	// überschritten werden und "SupportHighResolutions" den Wert "false" aufweist, wird die Dimensionen um
	// 50 % skaliert.
	static const float DpiThreshold = 192.0f;		// 200 % der Standarddesktopanzeige skaliert.
	static const float WidthThreshold = 1920.0f;	// 1080p Breite skaliert.
	static const float HeightThreshold = 1080.0f;	// 1080p Höhe skaliert.
};

// Für die Berechnung der Bildschirmdrehungen verwendete Konstanten.
namespace ScreenRotation
{
	// 0-Grad-Drehung um die Z-Achse
	static const XMFLOAT4X4 Rotation0( 
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 90-Grad-Drehung um die Z-Achse
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 180-Grad-Drehung um die Z-Achse
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// 270-Grad-Drehung um die Z-Achse
	static const XMFLOAT4X4 Rotation270( 
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Konstruktor für DeviceResources.
DX::DeviceResources::DeviceResources() : 
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_compositionScaleX(1.0f),
	m_compositionScaleY(1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Konfiguriert Ressourcen, die nicht vom Direct3D-Gerät abhängig sind.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Direct2D-Ressourcen initialisieren.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// Wenn es sich bei dem Projekt um eine Debugversion handelt, Direct2D-Debugging über SDK Layers aktivieren.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Die Direct2D-Factory initialisieren.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// Die DirectWrite-Factory initialisieren.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Die Windows Imaging Component-Factory (WIC) initialisieren.
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Konfiguriert das Direct3D-Gerät und speichert die entsprechenden Handles und den Gerätekontext.
void DX::DeviceResources::CreateDeviceResources() 
{
	// Dieses Flag fügt die Unterstützung für Oberflächen hinzu, die über eine andere Farbchannelsortierung
	// als die API-Standardeinstellung verfügen. Dies ist für die Kompatibilität mit Direct2D erforderlich.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// Wenn es sich bei dem Projekt um eine Debugversion handelt, Debugging über SDK Layers mit diesem Flag aktivieren.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// In diesem Array wird der Satz von Hardwarefunktionsebenen für DirectX definiert, der von dieser Anwendung unterstützt wird.
	// Darauf achten, dass die Reihenfolge beibehalten werden sollte.
	// Nicht vergessen, in der Beschreibung die mindestens erforderliche Funktionsebene der Anwendung
	// zu deklarieren.  Es wird vorausgesetzt, dass alle Anwendungen 9.1 unterstützen, sofern nichts anderes angegeben ist.
	D3D_FEATURE_LEVEL featureLevels[] = 
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Das Direct3D 11 API-Geräteobjekt und den entsprechenden Kontext erstellen.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// "nullptr" angeben, um den Standardadapter zu verwenden.
		D3D_DRIVER_TYPE_HARDWARE,	// Mit dem Hardwaregrafiktreiber ein Gerät erstellen.
		0,							// Sollte 0 sein, wenn der Treiber nicht D3D_DRIVER_TYPE_SOFTWARE ist.
		creationFlags,				// Debug- und Direct2D-Kompatibilitätsflags festlegen.
		featureLevels,				// Liste der von dieser App unterstützten Funktionsebenen.
		ARRAYSIZE(featureLevels),	// Größe der oben angeführten Liste.
		D3D11_SDK_VERSION,			// Dies für Windows Store-Apps immer auf D3D11_SDK_VERSION festlegen.
		&device,					// Gibt das erstellte Direct3D-Gerät zurück.
		&m_d3dFeatureLevel,			// Gibt die Funktionsebene des erstellten Geräts zurück.
		&context					// Gibt den unmittelbaren Kontext des Geräts zurück.
		);

	if (FAILED(hr))
	{
		// Wenn die Initialisierung fehlschlägt, auf das WARP-Gerät zurückgreifen.
		// Weitere Informationen zu WARP finden Sie unter: 
		// http://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Anstelle eines WARP- ein Hardwaregerät erstellen.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Zeiger zum Direct3D 11.3-API-Gerät und dem unmittelbaren Kontext speichern.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Das Direct2D-Geräteobjekt und den entsprechenden Kontext erstellen.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);
}

// Diese Ressourcen müssen bei jeder Änderung der Fenstergröße erneut erstellt werden.
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// Löschen Sie den für die vorherige Fenstergröße spezifischen Kontext.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// Die Breite und Höhe der Swapchain muss auf der Breite und Höhe des Fensters
	// mit der nativen Ausrichtung beruhen. Wenn das Fenster nicht im nativen
	// ausgerichtet ist, müssen die Abmessungen umgekehrt werden.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// Die Größe anpassen, wenn die Swapchain bereits vorhanden ist.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Doppelt gepufferte Swapchain.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// Wenn das Gerät aus einem beliebigen Grund entfernt wurde, müssen ein neues Gerät und eine Swapchain erstellt werden.
			HandleDeviceLost();

			// Nun wurde alles eingerichtet. Diese Methode nicht mehr ausführen. HandleDeviceLost gibt diese Methode erneut ein 
			// und das neue Gerät ordnungsgemäß einrichten.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// Andernfalls mit dem Adapter, der auch vom vorhandenen Direct3D-Gerät verwendet wird, eine neue erstellen.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Größe des Fensters anpassen.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// Dies ist das am häufigsten verwendete Swapchain-Format.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// Kein Mehrfachsampling verwenden.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Doppelte Pufferung verwenden, um die Wartezeit zu minimieren.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Alle Windows Store-Apps müssen "_FLIP_ SwapEffects" verwenden.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// Diese Sequenz ruft die zum Erstellen des oben angeführten Direct3D-Geräts verwendete DXGI-Factory ab.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		// Beim Verwenden von XAML-Interop muss die Swapchain für die Komposition erstellt werden.
		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForComposition(
				m_d3dDevice.Get(),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);

		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// Swapchain zu SwapChainPanel zuordnen
		// UI-Änderungen müssen zurück in den UI-Thread verteilt werden
		m_swapChainPanel->Dispatcher->RunAsync(CoreDispatcherPriority::High, ref new DispatchedHandler([=]()
		{
			// Dahinterliegende native Schnittstelle für SwapChainPanel abrufen
			ComPtr<ISwapChainPanelNative> panelNative;
			DX::ThrowIfFailed(
				reinterpret_cast<IUnknown*>(m_swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative))
				);

			DX::ThrowIfFailed(
				panelNative->SetSwapChain(m_swapChain.Get())
				);
		}, CallbackContext::Any));

		// Sicherstellen, dass DXGI jeweils nur einen Frame in die Warteschlange stellt. Hierdurch wird die Wartezeit verringert und gleichzeitig
		// sichergestellt, dass die Anwendung nur nach jeder VSync rendert und der Stromverbrauch minimiert wird.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// Die richtige Ausrichtung der Swapchain festlegen sowie 2D- und
	// 3D-Matrixtransformationen generieren, um die gedrehte Swapchain zu rendern.
	// Der Drehwinkel für 2D- und 3D-Transformationen ist unterschiedlich!
	// Dies liegt am unterschiedlichen Koordinatenraum. Außerdem wird
	// die 3D-Matrix explizit angegeben, um Rundungsfehler zu vermeiden.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Umgekehrte Skalierung in der Swapchain einrichten
	DXGI_MATRIX_3X2_F inverseScale = { 0 };
	inverseScale._11 = 1.0f / m_effectiveCompositionScaleX;
	inverseScale._22 = 1.0f / m_effectiveCompositionScaleY;
	ComPtr<IDXGISwapChain2> spSwapChain2;
	DX::ThrowIfFailed(
		m_swapChain.As<IDXGISwapChain2>(&spSwapChain2)
		);

	DX::ThrowIfFailed(
		spSwapChain2->SetMatrixTransform(&inverseScale)
		);

	// Eine Renderzielansicht des Swapchain-Hintergrundpuffers erstellen.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// Eine Ansicht der Tiefenschablone erstellen, um diese ggf. zum 3D-Rendering zu verwenden.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // Diese Tiefenschablonenansicht verfügt nur über eine Textur.
		1, // Eine einzelne Mipmap-Ebene verwenden.
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);

	// Einen 3D-Rendering-Viewport mit dem gesamten Fenster als Ziel erstellen.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Eine Direct2D-Zielbitmap erstellen, die dem
	// Swapchain-Hintergrundpuffer zugeordnet ist, und diese als aktuelles Ziel festlegen.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Antialiasing für Graustufentext wird für alle Windows Store-Apps empfohlen.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// Ermittelt die Dimensionen des Renderziels und bestimmt, ob eine zentrale Herunterskalierung erfolgt.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;
	m_effectiveCompositionScaleX = m_compositionScaleX;
	m_effectiveCompositionScaleY = m_compositionScaleY;

	// Damit die Akkubetriebsdauer auf Geräten mit hoher Auflösung verbessert wird, rendern Sie für ein kleineres Renderziel,
	// und erlauben Sie dem Grafikprozessor die Skalierung der Ausgabe, wenn diese dargestellt wird.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// Wenn das Gerät im Hochformat anzeigt, ist die Höhe größer als die Breite. Vergleichen Sie die
		// größere Dimension mit dem Breitenschwellenwert und die kleinere Dimension
		// mit dem Höhenschwellenwert.
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// Zum Skalieren der App wird der effektive DPI-Wert geändert. Die logische Größe ändert sich nicht.
			m_effectiveDpi /= 2.0f;
			m_effectiveCompositionScaleX /= 2.0f;
			m_effectiveCompositionScaleY /= 2.0f;
		}
	}

	// Berechnet die erforderliche Renderzielgröße in Pixeln.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Verhindern, dass DirectX-Inhalte der Größe NULL erstellt werden.
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

// Diese Methode wird aufgerufen, wenn die XAML-Steuerung erstellt (oder neu erstellt) wird.
void DX::DeviceResources::SetSwapChainPanel(SwapChainPanel^ panel)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_swapChainPanel = panel;
	m_logicalSize = Windows::Foundation::Size(static_cast<float>(panel->ActualWidth), static_cast<float>(panel->ActualHeight));
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_compositionScaleX = panel->CompositionScaleX;
	m_compositionScaleY = panel->CompositionScaleY;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// Diese Methode wird im Ereignishandler für das SizeChanged-Ereignis aufgerufen.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// Diese Methode wird im Ereignishandler für das DpiChanged-Ereignis aufgerufen.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;
		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// Diese Methode wird im Ereignishandler für das OrientationChanged-Ereignis aufgerufen.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Diese Methode wird im Ereignishandler für das CompositionScaleChanged-Ereignis aufgerufen.
void DX::DeviceResources::SetCompositionScale(float compositionScaleX, float compositionScaleY)
{
	if (m_compositionScaleX != compositionScaleX ||
		m_compositionScaleY != compositionScaleY)
	{
		m_compositionScaleX = compositionScaleX;
		m_compositionScaleY = compositionScaleY;
		CreateWindowSizeDependentResources();
	}
}

// Diese Methode wird im Ereignishandler für das DisplayContentsInvalidated-Ereignis aufgerufen.
void DX::DeviceResources::ValidateDevice()
{
	// Das D3D-Gerät ist nicht mehr gültig, wenn der Standardadapter geändert wird, nachdem das Gerät
	// erstellt wurde oder wenn das Gerät entfernt wurde.

	// Zunächst die Informationen zum Standardadapter bei Erstellung des Geräts abrufen.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory2> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// Anschließend die Informationen für den aktuellen Standardadapter abrufen.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// Wenn die Adapter-LUIDs nicht übereinstimmen oder das Gerät meldet, dass es entfernt wurde,
	// ein neues D3D-Gerät muss erstellt werden.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Verweise auf mit dem alten Gerät in Beziehung stehenden Ressourcen freigeben.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Ein neues Gerät und eine Swapchain erstellen.
		HandleDeviceLost();
	}
}

// Alle Geräteressourcen neu erstellen und auf den aktuellen Zustand zurücksetzen.
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Unsere DeviceNotify registrieren, um über Verlust oder Erstellung des Geräts benachrichtigt zu werden.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Diese Methode aufrufen, wenn die App anhält. Dies weist den Treiber darauf hin, dass die App 
// in einen Leerlaufzustand wechselt, und dass die temporären Puffer für die Verwendung durch andere Apps freigegeben werden.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// Die Inhalte der Swapchain auf dem Bildschirm anzeigen.
void DX::DeviceResources::Present() 
{
	// Das erste Argument weist DXGI an, bis zur VSync zu blockieren, sodass die Anwendung
	// bis zur nächsten VSync in den Standbymodus versetzt wird. Dadurch wird sichergestellt, dass beim Rendern von
	// Frames, die nie auf dem Display angezeigt werden, keine unnötigen Zyklen ausgeführt werden.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// Die Inhalte des Renderziels verwerfen.
	// Dies ist nur dann ein gültiger Vorgang, wenn die vorhandenen Inhalte vollständig
	// überschrieben werden. Wenn dirty- oder scroll-Rechtecke verwendet werden, sollte dieser Aufruf geändert werden.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// Die Inhalte der Tiefenschablone verwerfen.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	// Wenn das Gerät durch eine Verbindungstrennung oder ein Treiberupgrade entfernt wurde, müssen 
	// muss alle Geräteressourcen verwerfen.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// Diese Methode bestimmt die Drehung zwischen der nativen Ausrichtung des Anzeigegeräts und der
// aktuellen Bildschirmausrichtung.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Hinweis: NativeOrientation kann nur Landscape oder Portrait sein, obwohl
	// die DisplayOrientations-Enumeration hat andere Werte.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}