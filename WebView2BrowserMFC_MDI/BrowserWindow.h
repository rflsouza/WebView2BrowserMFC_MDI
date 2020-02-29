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

public:
	static wil::com_ptr<ICoreWebView2Environment> m_webViewEnvironment;	
	static BOOL InitInstance();
	
	BrowserWindow(HWND hWnd);
	~BrowserWindow();
	void Resize();
};

