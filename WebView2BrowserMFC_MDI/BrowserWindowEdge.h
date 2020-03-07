#pragma once
#include <afxwin.h>
#include "framework.h"

class BrowserWindowEdge :
	public CWnd
{
	wil::com_ptr<ICoreWebView2Host> m_host;
	wil::com_ptr<ICoreWebView2> m_webView;	

	std::wstring m_initialUri;
	std::function<void()> m_onWebViewFirstInitialized = nullptr;
	bool InitWebView();
	
	DECLARE_MESSAGE_MAP()

protected:
	// The events we register on the event source
	EventRegistrationToken m_WebMessageReceivedToken = {};
	EventRegistrationToken m_NavigationStartingToken = {};
	EventRegistrationToken m_NavigationCompletedToken = {};
	EventRegistrationToken m_ProcessFailedToken = {};
	EventRegistrationToken m_DevToolsSecurityToken = {};
	EventRegistrationToken m_DevToolsLogToken = {};

	Microsoft::WRL::ComPtr<ICoreWebView2WebMessageReceivedEventHandler> m_MessageBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2NavigationStartingEventHandler> m_NavigationStartingBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2NavigationCompletedEventHandler> m_NavigationCompletedBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2ProcessFailedEventHandler> m_ProcessFailedBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2DevToolsProtocolEventReceivedEventHandler> m_devToolsSecurityBroker;
	Microsoft::WRL::ComPtr<ICoreWebView2DevToolsProtocolEventReceivedEventHandler> m_devToolsLogBroker;

	HRESULT SetEventsAndBrokers();
	//Microsoft::WRL::Wrappers::RoInitializeWrapper initialize;
	Microsoft::WRL::Wrappers::Event NavigationCompletedEvent;

public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	BOOL ShowWindow(int nCmdShow);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg LRESULT OnRunAsync(WPARAM wParam, LPARAM lParam);

	static wil::com_ptr<ICoreWebView2Environment> m_webViewEnvironment;
	static BOOL InitInstance(HINSTANCE hInstance);

	BrowserWindowEdge();
	~BrowserWindowEdge();
	void RunAsync(std::function<void(void)> callback);

	void Init(std::wstring initialUri = L"https://www.bing.com/", std::function<void()> webviewCreatedCallback = nullptr);
	void Resize(RECT bounds = RECT());
	HRESULT Navigate(std::wstring url);
	/*	Initiates a navigation to htmlContent as source HTML of a new document.
	The htmlContent parameter may not be larger than 2 MB of characters. The origin of the new page will be about:blank. */
	HRESULT NavigateToString(std::wstring content);
	HRESULT PostJson(web::json::value jsonObj);
	/*	Add the provided JavaScript to a list of scripts that should be executed after the global object has been created,
		but before the HTML document has been parsed and before any other script included by the HTML document is executed. */
	HRESULT AddInitializeScript(std::wstring script, Microsoft::WRL::ComPtr<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler> handler = nullptr);
	// Execute JavaScript code from the javascript parameter in the current top level document rendered in the WebView.	
	HRESULT InjectScript(std::wstring script, Microsoft::WRL::ComPtr<ICoreWebView2ExecuteScriptCompletedHandler> handler = nullptr);

	static void CheckFailure(HRESULT hr, LPCWSTR errorMessage);
	static std::wstring GetAppDataDirectory();
	static std::wstring GetFullPathFor(LPCWSTR relativePath);
	static std::wstring GetFilePathAsURI(std::wstring fullPath);	
};

