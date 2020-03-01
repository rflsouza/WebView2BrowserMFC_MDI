#pragma once
#include "framework.h"

class BrowserWindow
{
	long m_Id;
	HINSTANCE m_hInst = nullptr;  // Current app instance
	HWND m_hWnd = nullptr;

	// The following is state that belongs with the webview, and should
	// be reinitialized along with it.  Everything here is undefined when
	// m_webView is null.	
	wil::com_ptr<ICoreWebView2Host> m_host;
	wil::com_ptr<ICoreWebView2> m_webView;

	bool InitWebView();

protected:
	EventRegistrationToken m_EventRegistrationToken = {};  // Token for the UI message handler in controls WebView
	Microsoft::WRL::ComPtr<ICoreWebView2WebMessageReceivedEventHandler> m_MessageBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2NavigationStartingEventHandler> m_NavigationStartingBroker;		
	
	//https://chromedevtools.github.io/devtools-protocol/tot/Log#type-LogEntry
	Microsoft::WRL::ComPtr<ICoreWebView2DevToolsProtocolEventReceivedEventHandler> m_devToolsSecurityBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2DevToolsProtocolEventReceivedEventHandler> m_devToolsLogBroker;
	
	void SetBrokers();

public:
	static wil::com_ptr<ICoreWebView2Environment> m_webViewEnvironment;	
	static BOOL InitInstance();
	
	BrowserWindow(HWND hWnd);
	~BrowserWindow();
	void Resize();
	HRESULT Navigate(std::wstring url);
	HRESULT PostJson(web::json::value jsonObj);

	static void CheckFailure(HRESULT hr, LPCWSTR errorMessage);
	static std::wstring GetAppDataDirectory();
	std::wstring GetFullPathFor(LPCWSTR relativePath);
};

