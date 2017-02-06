﻿//
// App.xaml.h
// Deklaration der App-Klasse
//

#pragma once

#include "App.g.h"
#include "DirectXPage.xaml.h"

namespace DxAppXAML
{
		/// <summary>
	/// Stellt das anwendungsspezifische Verhalten bereit, um die Standardanwendungsklasse zu ergänzen.
	/// </summary>
	ref class App sealed
	{
	public:
		App();
		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;

	private:
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnResuming(Platform::Object ^sender, Platform::Object ^args);
		DirectXPage^ m_directXPage;
	};
}
