#include "pch.h"
#include "framework.h"
#include "BrowserWindow.h"

using namespace Microsoft::WRL;

wil::com_ptr<ICoreWebView2Environment> BrowserWindow::m_webViewEnvironment = nullptr;

BOOL BrowserWindow::InitInstance()
{				
	HRESULT hr = CreateCoreWebView2EnvironmentWithDetails(nullptr, nullptr,
		L"", Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
	{
		if (!SUCCEEDED(result))
		{
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				MessageBox(
					nullptr,
					L"Couldn't find Edge installation. "
					"Do you have a version installed that's compatible with this "
					"WebView2 SDK version?",
					nullptr, MB_OK);
			}			
		}
		RETURN_IF_FAILED(result);

		m_webViewEnvironment = env;		

		return result;
	}).Get());

	if (!SUCCEEDED(hr))
	{
		OutputDebugString(L"WebView2Environment creation failed\n");
		return FALSE;
	}

	return TRUE;
}

BrowserWindow::BrowserWindow(HWND hWnd)
{	
	m_Id = ++g_browsers;
	TRACE("\r\n\t+++ [%ld] %s - %p id:%ld\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id);
	
	m_hWnd = hWnd;
	InitWebView();
}

BrowserWindow::~BrowserWindow()
{	
	TRACE("\r\n\t--- [%ld] %s - %p id:%ld\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id);
	if (m_host)
	{
		m_host->Close();
		m_host = nullptr;
		m_webView = nullptr;
	}
}

void BrowserWindow::Resize()
{
	if (m_hWnd != nullptr && m_host != nullptr)
	{
		RECT bounds;
		GetClientRect(m_hWnd, &bounds);
		m_host->put_Bounds(bounds);
		m_host->put_IsVisible(TRUE);
	}	
}

bool BrowserWindow::InitWebView()
{
	if (m_webViewEnvironment == nullptr) 
		return false;

	// Create a CoreWebView2Host and get the associated CoreWebView2 whose parent is the main window hWnd
	m_webViewEnvironment->CreateCoreWebView2Host(m_hWnd, Callback<ICoreWebView2CreateCoreWebView2HostCompletedHandler>(
		[this](HRESULT result, ICoreWebView2Host* host) -> HRESULT {
		if (host != nullptr) {
			m_host = host;
			m_host->get_CoreWebView2(&m_webView);
		}

		// Add a few settings for the webview
		// this is a redundant demo step as they are the default settings values
		ICoreWebView2Settings* Settings;
		m_webView->get_Settings(&Settings);
		Settings->put_IsScriptEnabled(TRUE);
		Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
		Settings->put_IsWebMessageEnabled(TRUE);

		// Resize WebView to fit the bounds of the parent window
		Resize();

		// Schedule an async task to navigate to Bing		
		std::wstring url = _T("https://www.bing.com/search?q=teste+") + std::to_wstring(m_Id) + _T("+") + std::to_wstring((long)m_hWnd);
		m_webView->Navigate(url.c_str());

		// Step 4 - Navigation events


		// Step 5 - Scripting


		// Step 6 - Communication between host and web content


		return S_OK;
	}).Get());

	return false;
}

