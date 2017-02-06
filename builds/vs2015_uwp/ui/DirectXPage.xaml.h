//
// DirectXPage.xaml.h
// Deklaration der DirectXPage-Klasse.
//

#pragma once

#include "DirectXPage.g.h"

#include "DeviceResources.h"
#include "DxAppXAMLMain.h"

namespace DxAppXAML
{
	/// <summary>
	/// Eine Seite, die ein DirectX SwapChainPanel hostet.
	/// </summary>
	public ref class DirectXPage sealed
	{
	public:
		DirectXPage();
		virtual ~DirectXPage();

		void SaveInternalState(Windows::Foundation::Collections::IPropertySet^ state);
		void LoadInternalState(Windows::Foundation::Collections::IPropertySet^ state);

	private:
		// Ereignishandler für XAML-Rendering auf niedriger Ebene.
		void OnRendering(Platform::Object^ sender, Platform::Object^ args);

		// Ereignishandler für Fenster.
		void OnVisibilityChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::VisibilityChangedEventArgs^ args);

		// DisplayInformation-Ereignishandler.
		void OnDpiChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnOrientationChanged(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);
		void OnDisplayContentsInvalidated(Windows::Graphics::Display::DisplayInformation^ sender, Platform::Object^ args);

		// Andere Ereignishandler.
		void AppBarButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void OnCompositionScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel^ sender, Object^ args);
		void OnSwapChainPanelSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);

		// Unsere unabhängige Eingabe auf einem Arbeitsthread im Hintergrund nachverfolgen
		Windows::Foundation::IAsyncAction^ m_inputLoopWorker;
		Windows::UI::Core::CoreIndependentInputSource^ m_coreInput;

		// Behandlungsfunktionen für unabhängige Eingabe.
		void OnPointerPressed(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerMoved(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);
		void OnPointerReleased(Platform::Object^ sender, Windows::UI::Core::PointerEventArgs^ e);

		// Zum Rendern des DirectX-Inhalts im XAML-Seitenhintergrund verwendete Ressourcen.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;
		std::unique_ptr<DxAppXAMLMain> m_main; 
		bool m_windowVisible;
	};
}

