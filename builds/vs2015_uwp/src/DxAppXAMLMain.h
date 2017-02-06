#pragma once

#include "DeviceResources.h"

// Rendert Direct2D- und 3D-Inhalt auf dem Bildschirm.
namespace DxAppXAML
{
	class DxAppXAMLMain : public DX::IDeviceNotify
	{
	public:
		DxAppXAMLMain(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~DxAppXAMLMain();
		void CreateWindowSizeDependentResources();
		void StartTracking() { }
		void TrackingUpdate(float positionX) { m_pointerLocationX = positionX; }
		void StopTracking() { }
		bool IsTracking() { return false; }
		void StartRenderLoop();
		void StopRenderLoop();
		Concurrency::critical_section& GetCriticalSection() { return m_criticalSection; }

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		void ProcessInput();
		void Update();
		bool Render();

		// Zeiger in den Geräteressourcen zwischengespeichert.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// TODO: Mit Ihren eigenen Inhaltsrenderern ersetzen.

		Windows::Foundation::IAsyncAction^ m_renderLoopWorker;
		Windows::Foundation::IAsyncAction^ m_updateLoopWorker;
		Concurrency::critical_section m_criticalSection;

		// Aktuelle Eingabezeigerposition nachverfolgen.
		float m_pointerLocationX;

		Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_sceneBitmap;
		Microsoft::WRL::ComPtr<ID2D1DrawingStateBlock1> m_stateBlock;
		D2D1_SIZE_U m_sceneBitmapSize = { 320, 240 };
	};
}