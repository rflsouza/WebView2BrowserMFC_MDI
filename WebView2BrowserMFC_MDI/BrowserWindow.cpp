#include "pch.h"
#include "framework.h"
#include "BrowserWindow.h"

using namespace Microsoft::WRL;

WCHAR BrowserWindow::s_windowClass[] = { 0 };
wil::com_ptr<ICoreWebView2Environment> BrowserWindow::m_webViewEnvironment = nullptr;

//#pragma comment(lib, "runtimeobject.lib") //RoInitializeWrapper

HRESULT BrowserWindow::SetEventsAndBrokers()
{
	// Set the message broker for the webview. This will capture messages from web content.
	m_MessageBroker = Callback<ICoreWebView2WebMessageReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* eventArgs) -> HRESULT
	{
		wil::unique_cotaskmem_string jsonString;
		CheckFailure(eventArgs->get_WebMessageAsJson(&jsonString), L"");
		web::json::value jsonObj = web::json::value::parse(jsonString.get());

		TRACE("\r\n[%ld] %p id:%ld MessageBroker %S\r\n", GetCurrentThreadId(), this, m_Id, jsonString.get());

		if (!jsonObj.has_field(L"message"))
		{
			OutputDebugStringW(L"No message code provided\n");
			return S_OK;
		}

		if (!jsonObj.has_field(L"args"))
		{
			OutputDebugStringW(L"The message has no args field\n");
			return S_OK;
		}

		int message = jsonObj.at(L"message").as_integer();
		web::json::value args = jsonObj.at(L"args");

		switch (message)
		{
		case 1:
		{
			std::wstring uri(args.at(L"uri").as_string());
			std::wstring browserScheme(L"browser://");

			if (uri.substr(0, browserScheme.size()).compare(browserScheme) == 0)
			{
				// No encoded search URI
				std::wstring path = uri.substr(browserScheme.size());
				if (path.compare(L"favorites") == 0 ||
					path.compare(L"settings") == 0 ||
					path.compare(L"history") == 0)
				{
					std::wstring filePath(L"wvbrowser_ui\\content_ui\\");
					filePath.append(path);
					filePath.append(L".html");
					std::wstring fullPath = GetFullPathFor(filePath.c_str());
					CheckFailure(Navigate(fullPath.c_str()), L"Can't navigate to browser page.");
				}
				else
				{
					OutputDebugStringW(L"Requested unknown browser page\n");
				}
			}
			else if (!SUCCEEDED(Navigate(uri.c_str())))
			{
				CheckFailure(Navigate(args.at(L"encodedSearchURI").as_string().c_str()), L"Can't navigate to requested page.");
			}
		}
		break;
		case 2:
		{
			// Forward back to requesting tab
			size_t tabId = args.at(L"tabId").as_number().to_uint32();
			jsonObj[L"args"].erase(L"tabId");

			CheckFailure(PostJson(jsonObj), L"Requesting history failed.");
		}
		break;
		default:
		{
			OutputDebugStringW(L"Unexpected message\n");
		}
		break;
		}

		return S_OK;
	});
	RETURN_IF_FAILED(m_webView->add_WebMessageReceived(m_MessageBroker.Get(), &m_WebMessageReceivedToken));

	m_NavigationStartingBroker = Callback<ICoreWebView2NavigationStartingEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs * args) -> HRESULT
	{
		wil::unique_cotaskmem_string uri;
		RETURN_IF_FAILED(args->get_Uri(&uri));
		TRACE("\r\n[%ld] %p id:%ld NavigationStartingBroker %S\r\n", GetCurrentThreadId(), this, m_Id, uri.get());

		std::wstring source(uri.get());
		if (source.substr(0, 5) != L"https"  && source.substr(0, 6) != L"about:" && source.substr(0, 8) != L"file:///") {
			args->put_Cancel(true);

			BOOL userInitiated;
			RETURN_IF_FAILED(args->get_IsUserInitiated(&userInitiated));
			static const PCWSTR htmlContent =
				L"<h1>Domain Blocked</h1>"
				L"<p>You've attempted to navigate to a domain in the blocked "
				L"sites list. Press back to return to the previous page.</p>";
			RETURN_IF_FAILED(webview->NavigateToString(htmlContent));
		}
		return S_OK;
	});
	RETURN_IF_FAILED(m_webView->add_NavigationStarting(m_NavigationStartingBroker.Get(), &m_NavigationStartingToken));


	m_NavigationCompletedBroker = Callback<ICoreWebView2NavigationCompletedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2NavigationCompletedEventArgs * args) -> HRESULT
	{
		BOOL isSuccess = FALSE;
		RETURN_IF_FAILED(args->get_IsSuccess(&isSuccess));
		TRACE("\r\n[%ld] %p id:%ld NavigationCompleted %d\r\n", GetCurrentThreadId(), this, m_Id, isSuccess);

		SetEvent(NavigationCompletedEvent.Get());

		if (m_onWebViewFirstInitialized)
		{
			m_onWebViewFirstInitialized();
			m_onWebViewFirstInitialized = nullptr;
		}

		return S_OK;
	});
	RETURN_IF_FAILED(m_webView->add_NavigationCompleted(m_NavigationCompletedBroker.Get(), &m_NavigationCompletedToken));

	m_ProcessFailedBroker = Callback<ICoreWebView2ProcessFailedEventHandler>(
		[this](ICoreWebView2* sender,
			ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT
	{
		CORE_WEBVIEW2_PROCESS_FAILED_KIND failureType;
		RETURN_IF_FAILED(args->get_ProcessFailedKind(&failureType));
		if (failureType == CORE_WEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED)
		{
			int button = ::MessageBoxW(
				m_hWnd,
				L"Browser process exited unexpectedly.  Recreate webview?",
				L"Browser process exited",
				MB_YESNO);
			if (button == IDYES)
			{
				//m_appWindow->ReinitializeWebView();
			}
		}
		else if (failureType == CORE_WEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE)
		{
			int button = ::MessageBoxW(
				m_hWnd,
				L"Browser render process has stopped responding.  Recreate webview?",
				L"Web page unresponsive", MB_YESNO);
			if (button == IDYES)
			{
				//m_appWindow->ReinitializeWebView();
			}
		}
		return S_OK;
	}).Get();
	RETURN_IF_FAILED(m_webView->add_ProcessFailed(m_ProcessFailedBroker.Get(), &m_ProcessFailedToken));

	// Enable listening for security events to update secure icon
	wil::com_ptr<ICoreWebView2DevToolsProtocolEventReceiver> devToolsProtocolEventReceiver;

	m_devToolsSecurityBroker = Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args) -> HRESULT
	{
		wil::unique_cotaskmem_string parameterObjectAsJson;
		RETURN_IF_FAILED(args->get_ParameterObjectAsJson(&parameterObjectAsJson));
		TRACE("\r\n[%ld] %p id:%ld devToolsSecurityBroker %S\r\n", GetCurrentThreadId(), this, m_Id, parameterObjectAsJson.get());

		return S_OK;
	}).Get();
	RETURN_IF_FAILED(m_webView->CallDevToolsProtocolMethod(L"Security.enable", L"{}", nullptr));
	BrowserWindow::CheckFailure(m_webView->GetDevToolsProtocolEventReceiver(L"Security.securityStateChanged", &devToolsProtocolEventReceiver), L"");
	devToolsProtocolEventReceiver->add_DevToolsProtocolEventReceived(m_devToolsSecurityBroker.Get(), &m_DevToolsSecurityToken);
	
	//https://chromedevtools.github.io/devtools-protocol/tot/Log#type-LogEntry
	m_devToolsLogBroker = Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args) -> HRESULT
	{
		wil::unique_cotaskmem_string parameterObjectAsJson;
		RETURN_IF_FAILED(args->get_ParameterObjectAsJson(&parameterObjectAsJson));
		TRACE("\r\n[%ld] %p id:%ld devToolsLogBroker %S\r\n", GetCurrentThreadId(), this, m_Id, parameterObjectAsJson.get());

		return S_OK;
	}).Get();
	RETURN_IF_FAILED(m_webView->CallDevToolsProtocolMethod(L"Log.enable", L"{}", nullptr));
	BrowserWindow::CheckFailure(m_webView->GetDevToolsProtocolEventReceiver(L"Log.entryAdded", &devToolsProtocolEventReceiver), L"");
	devToolsProtocolEventReceiver->add_DevToolsProtocolEventReceived(m_devToolsLogBroker.Get(), &m_DevToolsLogToken);


	return S_OK;
}

BOOL BrowserWindow::InitInstance(HINSTANCE hInstance)
{
	if (!SUCCEEDED(RegisterClass(hInstance)) ){
		return false;
	}

	// Get directory for user data. This will be kept separated from the
   // directory for the browser UI data.
	std::wstring userDataDirectory = GetAppDataDirectory();
	userDataDirectory.append(L"\\User Data");

	std::wstring additionalBrowserArguments; 
	//additionalBrowserArguments = L"--auto-open-devtools-for-tabs";

	HRESULT hr = CreateCoreWebView2EnvironmentWithDetails(
		nullptr, 
		userDataDirectory.c_str(),
		additionalBrowserArguments.c_str(), 
		Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
			[](HRESULT result, ICoreWebView2Environment* env) -> HRESULT
	{
		if (!SUCCEEDED(result))
		{
			if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				::MessageBoxW(
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
		OutputDebugStringW(L"WebView2Environment creation failed\n");
		return FALSE;
	}

	return TRUE;
}

BrowserWindow::BrowserWindow(HINSTANCE hInstance, HWND parentHWnd, std::wstring initialUri, std::function<void()> webviewCreatedCallback)
	: m_parentHWnd(parentHWnd), m_initialUri(initialUri), m_onWebViewFirstInitialized(webviewCreatedCallback),
	/*initialize(RO_INIT_MULTITHREADED),*/
	NavigationCompletedEvent(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS))
{
	m_Id = ++g_browsers;
	TRACE("\r\n\t+++ [%ld] %s - %p id:%ld\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id);
	
	/*m_hWnd = CreateWindowExW(
		WS_EX_CONTROLPARENT, s_windowClass, L"Title Browser", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
		CW_USEDEFAULT, 0, parentHWnd, nullptr, hInstance, nullptr);*/
	m_hWnd = CreateWindowW(s_windowClass, L"Title Browser", WS_CHILD, 0, 0, 10, 10, parentHWnd, nullptr, hInstance, nullptr);

	if (!m_hWnd)
	{
		DWORD erro = GetLastError();
		TRACE("\r\n\t!!!!! [%ld] %s - %p id:%ld GetLastError:ld\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id, erro);
		return ;
	}
	
	TRACE("\r\n\t!!!!! [%ld] %s - %p id:%ld hWnd:%p parentHWnd:%p\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id, m_hWnd, parentHWnd);

	ShowWindow(m_hWnd, SW_SHOW);
	UpdateWindow(m_hWnd);

	SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);

	//InitWebView();
	RunAsync([this] {
		InitWebView();
	});	
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

	if (m_hWnd) {
		DestroyWindow(m_hWnd);
		m_hWnd = nullptr;
	}
}

void BrowserWindow::RunAsync(std::function<void()> callback)
{
	auto* task = new std::function<void()>(callback);
	PostMessage(m_hWnd, WM_APP, reinterpret_cast<WPARAM>(task), 0);
}

void BrowserWindow::Resize(RECT bounds)
{
	//if (m_hWnd != nullptr && m_host != nullptr)
	//{
	//	RECT bounds;
	//	GetClientRect(m_hWnd, &bounds);
	//	m_host->put_Bounds(bounds);
	//	m_host->put_IsVisible(TRUE);
	//}
	if (m_parentHWnd != nullptr && m_host != nullptr)
	{		
		if (bounds.bottom == 0 && bounds.top == 0, bounds.left == 0, bounds.right == 0) {
			bounds.bottom += 10;
			bounds.top += 10;
			bounds.left += 10;
			bounds.right += 10;
			::GetClientRect(m_parentHWnd, &bounds);
		}		
		m_host->put_Bounds(bounds);
		m_host->put_IsVisible(TRUE);		

		ShowWindow(m_hWnd, SW_SHOWMAXIMIZED);
		UpdateWindow(m_hWnd);	
	}
}

HRESULT BrowserWindow::Navigate(std::wstring url)
{
	if (m_webView == nullptr) 
		return ERROR_INVALID_HANDLE;
	
	ResetEvent(NavigationCompletedEvent.Get());
	SetEvent(NavigationCompletedEvent.Get());	

	auto result = m_webView->Navigate(url.c_str());

	WaitForSingleObjectEx(NavigationCompletedEvent.Get(), INFINITE, FALSE);
	return result;
}

HRESULT BrowserWindow::NavigateToString(std::wstring content)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	return m_webView->NavigateToString(content.c_str());
}

HRESULT BrowserWindow::PostJson(web::json::value jsonObj)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	utility::stringstream_t stream;
	jsonObj.serialize(stream);

	return m_webView->PostWebMessageAsJson(stream.str().c_str());
}

HRESULT BrowserWindow::AddInitializeScript(std::wstring script, Microsoft::WRL::ComPtr<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler> handler)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	if (script.empty())
		return ERROR_INVALID_PARAMETER;

	if (handler == nullptr) {
		handler = Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
			[this](HRESULT error, PCWSTR id) -> HRESULT
		{
			if (error != S_OK) {
				TRACE("\r\n\t+++ [%ld] %s - %p id:%ld AddScriptToExecuteOnDocumentCreated failed Result %ld \r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id, error);
			}
			TRACE("\r\n\t+++ [%ld] %s - %p id:%ld AddScriptToExecuteOnDocumentCreated Id %s \r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id, id);
			return error;
		});
	}

	return m_webView->AddScriptToExecuteOnDocumentCreated(script.c_str(),handler.Get());	
}

HRESULT BrowserWindow::InjectScript(std::wstring script, Microsoft::WRL::ComPtr <ICoreWebView2ExecuteScriptCompletedHandler> handler)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	if (script.empty())
		return ERROR_INVALID_PARAMETER;

	if (handler == nullptr) {
		handler = Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
			[this](HRESULT error, PCWSTR result) -> HRESULT
		{
			if (error != S_OK) {
				TRACE("\r\n\t+++ [%ld] %s - %p id:%ld ExecuteScript failed Result %ld \r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id, error);
			}
			TRACE("\r\n\t+++ [%ld] %s - %p id:%ld ExecuteScript Result %S \r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id, result);
			return error;
		});
	}

	return m_webView->ExecuteScript(script.c_str(), handler.Get());
	
}

bool BrowserWindow::InitWebView()
{
	if (m_webViewEnvironment == nullptr)
		return false;

	// Create a CoreWebView2Host and get the associated CoreWebView2 whose parent is the main window hWnd
	HRESULT hr = m_webViewEnvironment->CreateCoreWebView2Host(m_hWnd, Callback<ICoreWebView2CreateCoreWebView2HostCompletedHandler>(
		[&](HRESULT result, ICoreWebView2Host* host) -> HRESULT {

		if (!SUCCEEDED(result))
		{
			OutputDebugStringW(L"WebView creation failed\n");
			return result;
		}

		if (host != nullptr) {
			m_host = host;			
			CheckFailure(m_host->get_CoreWebView2(&m_webView), L"");
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

		// Register events	
		RETURN_IF_FAILED(SetEventsAndBrokers());

		// Schedule an async task to navigate to Bing		
		/*std::wstring url = _T("https://www.bing.com/search?q=teste+") + std::to_wstring(m_Id) + _T("+") + std::to_wstring((long)m_hWnd);				
		Navigate(url.c_str());*/

		if (!m_initialUri.empty())
		{
			Navigate(m_initialUri.c_str());
		}

		////move to NavigationCompleted
		//if (m_onWebViewFirstInitialized)
		//{
		//	m_onWebViewFirstInitialized();
		//	m_onWebViewFirstInitialized = nullptr;
		//}

		return S_OK;
	}).Get());
	
	if (!SUCCEEDED(hr))
	{
		OutputDebugStringW(L"CreateCoreWebView2Host  failed\n");
		return false;
	}

	return true;
}

//
//  FUNCTION: RegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM BrowserWindow::RegisterClass(_In_ HINSTANCE hInstance)
{	
	// Initialize window class string
	LoadStringW(hInstance, AFX_IDS_APP_TITLE, s_windowClass, MAX_LOADSTRING);
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProcStatic;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(AFX_IDS_APP_TITLE));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(AFX_IDS_APP_TITLE);
	wcex.lpszClassName = s_windowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));

	return RegisterClassExW(&wcex);
}

//
//  FUNCTION: WndProcStatic(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Redirect messages to approriate instance or call default proc
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK BrowserWindow::WndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// Get the ptr to the BrowserWindow instance who created this hWnd.
	// The pointer was set when the hWnd was created during InitInstance.
	BrowserWindow* browser_window = reinterpret_cast<BrowserWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (browser_window != nullptr)
	{
		return browser_window->WndProc(hWnd, message, wParam, lParam);  // Forward message to instance-aware WndProc
	}
	else
	{
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for each browser window instance.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK BrowserWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{

	case WM_SIZE:
	{
		Resize();		
	}
	break;
	case WM_CLOSE:
	{

	}
	break;
	case WM_NCDESTROY:
	{
		SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
		//delete this;
		PostQuitMessage(0);
	}
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = ::BeginPaint(hWnd, &ps);
		::EndPaint(hWnd, &ps);
	}
	break;
	case WM_APP:
	{
		auto* task = reinterpret_cast<std::function<void()>*>(wParam);
		(*task)();
		delete task;
		return true;
	}
	break;
	//case WM_PARENTNOTIFY:
	//{
	//	int wmId = LOWORD(wParam);
	//	HWND hwnd;
	//	RECT cRect;
	//	RECT Rect = { 0 };		
	//	
	//	switch (wmId)
	//	{
	//	case WM_CREATE:
	//		hwnd = (HWND)lParam;
	//		GetWindowRect(hwnd, &cRect);
	//		GetWindowRect(hWnd, &Rect);
	//		Rect.right = max(Rect.right, (cRect.right + 7));//Windows 10 has thin invisible borders on left, right, and bottom
	//		Rect.bottom =max(Rect.bottom, cRect.bottom + 7);
	//		SetWindowPos(hWnd, HWND_TOP, 0, 0, Rect.right - Rect.left, Rect.bottom - Rect.top, SWP_NOZORDER | SWP_NOMOVE | SWP_SHOWWINDOW);
	//		break;
	//	case WM_DESTROY:
	//		break;
	//	}
	//	return 0;
	//}
	//break;
	default:
	{
		return ::DefWindowProc(hWnd, message, wParam, lParam);
	}
	break;
	}
	return 0;
}

void BrowserWindow::CheckFailure(HRESULT hr, LPCWSTR errorMessage)
{
	if (FAILED(hr))
	{
		std::wstring message;
		if (!errorMessage || !errorMessage[0])
		{
			message = std::wstring(L"Something went wrong.");
		}
		else
		{
			message = std::wstring(errorMessage);
		}

		::MessageBoxW(nullptr, message.c_str(), nullptr, MB_OK);
	}
}

std::wstring BrowserWindow::GetAppDataDirectory()
{
	std::wstring s_title = L"WebView2Browser"; //pegar depois do aplication
	WCHAR path[MAX_PATH];
	std::wstring dataDirectory;
	HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_APPDATA, NULL, 0, path);
	if (SUCCEEDED(hr))
	{
		dataDirectory = std::wstring(path);
		dataDirectory.append(L"\\Microsoft\\");
	}
	else
	{
		dataDirectory = std::wstring(L".\\");
	}

	dataDirectory.append(s_title);
	return dataDirectory;
}

std::wstring BrowserWindow::GetFullPathFor(LPCWSTR relativePath)
{
	WCHAR path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::wstring pathName(path);

	std::size_t index = pathName.find_last_of(L"\\") + 1;
	pathName.replace(index, pathName.length(), relativePath);

	return pathName;
}

std::wstring BrowserWindow::GetFilePathAsURI(std::wstring fullPath)
{
	std::wstring fileURI;
	ComPtr<IUri> uri;
	DWORD uriFlags = Uri_CREATE_ALLOW_IMPLICIT_FILE_SCHEME;
	HRESULT hr = CreateUri(fullPath.c_str(), uriFlags, 0, &uri);

	if (SUCCEEDED(hr))
	{
		wil::unique_bstr absoluteUri;
		uri->GetAbsoluteUri(&absoluteUri);
		fileURI = std::wstring(absoluteUri.get());
	}

	return fileURI;
}
