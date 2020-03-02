#pragma once
#include "framework.h"

class BrowserWindow
{
	long m_Id;
	HINSTANCE m_hInst;  // Current app instance
	HWND m_parentHWnd = nullptr;
	HWND m_hWnd = nullptr;
	std::wstring m_initialUri;
	std::function<void()> m_onWebViewFirstInitialized = nullptr;

	// The following is state that belongs with the webview, and should
	// be reinitialized along with it.  Everything here is undefined when
	// m_webView is null.	
	wil::com_ptr<ICoreWebView2Host> m_host;
	wil::com_ptr<ICoreWebView2> m_webView;

	bool InitWebView();
	static ATOM RegisterClass(_In_ HINSTANCE hInstance);
	static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

protected:
	static WCHAR s_windowClass[MAX_LOADSTRING];  // The window class name	

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

public:
	static wil::com_ptr<ICoreWebView2Environment> m_webViewEnvironment;	
	static BOOL InitInstance(HINSTANCE hInstance);
	
	BrowserWindow(HINSTANCE hInstance, HWND parentHWnd, std::wstring initialUri = L"https://www.bing.com/", std::function<void()> webviewCreatedCallback = nullptr);
	~BrowserWindow();
	static BOOL LaunchWindow(_In_ HINSTANCE hInstance, _In_ int nCmdShow);
	void RunAsync(std::function<void(void)> callback);

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

