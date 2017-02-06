#pragma once

#include <ppltasks.h>	// Für create_task

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// In dieser Zeile einen Haltepunkt festlegen, um Fehler der Win32-API abzufangen.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// Funktion, die asynchron aus einer Binärdatei liest.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Wandelt eine Längenangabe in geräteunabhängigen Pixeln (Device-Independent Pixels, DIPs) in eine Längenangabe in physischen Pixeln um.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Auf nächste ganze Zahl runden.
	}

#if defined(_DEBUG)
	// Auf SDK Layer-Unterstützung überprüfen
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // Es muss kein echtes Hardwaregerät erstellt werden.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Auf die SDK Layers überprüfen.
			nullptr,                    // Beliebige Funktionsebene.
			0,
			D3D11_SDK_VERSION,          // Dies für Windows Store-Apps immer auf D3D11_SDK_VERSION festlegen.
			nullptr,                    // Die D3D-Gerätegerätereferenz muss nicht behalten werden.
			nullptr,                    // Die Funktionsebene muss nicht bekannt sein.
			nullptr                     // Die D3D-Gerätekontextreferenz muss nicht behalten werden.
			);

		return SUCCEEDED(hr);
	}
#endif
}
