#include "pch.h"
#include "BrowserWindowEdge.h"
#include <locale>
#include <codecvt>

using namespace Microsoft::WRL;


wil::com_ptr<ICoreWebView2Environment> BrowserWindowEdge::m_webViewEnvironment = nullptr;


BEGIN_MESSAGE_MAP(BrowserWindowEdge, CWnd)
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()
	ON_MESSAGE(WM_APP, &BrowserWindowEdge::OnRunAsync)
END_MESSAGE_MAP()


bool BrowserWindowEdge::InitWebView()
{
	if (m_webViewEnvironment == nullptr)
		return false;

	if (m_webViewController && m_webView)
	{
		// Resize WebView to fit the bounds of the parent window
		m_webViewController->put_IsVisible(TRUE);
		Resize();

		if (!m_initialUri.empty())
		{
			Navigate(m_initialUri.c_str());
		}
		return true;
	}
	
	HRESULT hr = m_webViewEnvironment->CreateCoreWebView2Controller(m_hWnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
		[&](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {

		if (!SUCCEEDED(result))
		{
			OutputDebugStringW(L"WebView creation failed\n");
			return result;
		}

		if (controller != nullptr) {
			m_webViewController = controller;
			CheckFailure(m_webViewController->get_CoreWebView2(&m_webView), L"");
		}

		// Add a few settings for the webview
		// this is a redundant demo step as they are the default settings values
		ICoreWebView2Settings* Settings;
		m_webView->get_Settings(&Settings);
		Settings->put_IsScriptEnabled(TRUE);
		Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
		Settings->put_IsWebMessageEnabled(TRUE);

		// Resize WebView to fit the bounds of the parent window
		m_webViewController->put_IsVisible(TRUE);
		Resize();		

		// Register events	
		RETURN_IF_FAILED(SetEventsAndBrokers());

		if (!m_initialUri.empty())
		{
			Navigate(m_initialUri.c_str());
		}

		return S_OK;
	}).Get());

	if (!SUCCEEDED(hr))
	{
		OutputDebugStringW(L"CreateCoreWebView2Controller  failed\n");
		return false;
	}

	return true;
}

HRESULT BrowserWindowEdge::SetEventsAndBrokers()
{
	// Set the message broker for the webview. This will capture messages from web content.
	m_MessageBroker = Callback<ICoreWebView2WebMessageReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* eventArgs) -> HRESULT
	{
		wil::unique_cotaskmem_string jsonString;
		CheckFailure(eventArgs->get_WebMessageAsJson(&jsonString), L"");
		web::json::value jsonObj = web::json::value::parse(jsonString.get());

		TRACE("\r\n[%ld] %p MessageBroker %S\r\n", GetCurrentThreadId(), this, jsonString.get());

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
		TRACE("\r\n[%ld] %p NavigationStartingBroker %S\r\n", GetCurrentThreadId(), this, uri.get());

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
		TRACE("\r\n[%ld] %p NavigationCompleted %d\r\n", GetCurrentThreadId(), this, isSuccess);

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
		COREWEBVIEW2_PROCESS_FAILED_KIND failureType;
		RETURN_IF_FAILED(args->get_ProcessFailedKind(&failureType));
		if (failureType == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED)
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
		else if (failureType == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE)
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
		TRACE("\r\n[%ld] %p devToolsSecurityBroker %S\r\n", GetCurrentThreadId(), this, parameterObjectAsJson.get());

		return S_OK;
	}).Get();
	RETURN_IF_FAILED(m_webView->CallDevToolsProtocolMethod(L"Security.enable", L"{}", nullptr));
	BrowserWindowEdge::CheckFailure(m_webView->GetDevToolsProtocolEventReceiver(L"Security.securityStateChanged", &devToolsProtocolEventReceiver), L"");
	devToolsProtocolEventReceiver->add_DevToolsProtocolEventReceived(m_devToolsSecurityBroker.Get(), &m_DevToolsSecurityToken);

	//https://chromedevtools.github.io/devtools-protocol/tot/Log#type-LogEntry
	m_devToolsLogBroker = Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args) -> HRESULT
	{
		wil::unique_cotaskmem_string parameterObjectAsJson;
		RETURN_IF_FAILED(args->get_ParameterObjectAsJson(&parameterObjectAsJson));
		TRACE("\r\n[%ld] %p devToolsLogBroker %S\r\n", GetCurrentThreadId(), this, parameterObjectAsJson.get());

		return S_OK;
	}).Get();
	RETURN_IF_FAILED(m_webView->CallDevToolsProtocolMethod(L"Log.enable", L"{}", nullptr));
	BrowserWindowEdge::CheckFailure(m_webView->GetDevToolsProtocolEventReceiver(L"Log.entryAdded", &devToolsProtocolEventReceiver), L"");
	devToolsProtocolEventReceiver->add_DevToolsProtocolEventReceived(m_devToolsLogBroker.Get(), &m_DevToolsLogToken);


	return S_OK;
}

void BrowserWindowEdge::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
	
	Resize();
}

BOOL BrowserWindowEdge::ShowWindow(int nCmdShow)
{
	//TRACE("\r\n[%p] BrowserWindowEdge::ShowWindow Show:%d  GetForegroundWindow:%p GetActiveWindow:%p\r\n", this, nCmdShow, GetForegroundWindow(), GetActiveWindow());
	//if (nCmdShow == SW_HIDE)
	//{
	//	if (m_webViewController && GetForegroundWindow() != GetActiveWindow())
	//		m_webViewController->put_IsVisible(FALSE);
	//}
	//else
	//{
	//	if (m_webViewController)
	//		m_webViewController->put_IsVisible(TRUE);
	//}		

	return CWnd::ShowWindow(nCmdShow);
}
void BrowserWindowEdge::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CWnd::OnShowWindow(bShow, nStatus);	

	auto foreground = GetForegroundWindow();
	auto me = GetActiveWindow();
	//TRACE("\r\n[%p] BrowserWindowEdge::OnShowWindow Show:%d  GetForegroundWindow:%p GetActiveWindow:%p\r\n", this, bShow, foreground, me);
	
	if (bShow == FALSE)
	{
		if (m_webViewController)
			if (foreground == me)
				m_webViewController->put_IsVisible(FALSE);
	}
	else
	{
		if (m_webViewController)
			m_webViewController->put_IsVisible(TRUE);
	}

}

afx_msg LRESULT BrowserWindowEdge::OnRunAsync(WPARAM wParam, LPARAM lParam)
{
	auto* task = reinterpret_cast<std::function<void()>*>(wParam);
	(*task)();
	delete task;

	return S_OK;
}

BOOL BrowserWindowEdge::InitInstance(HINSTANCE hInstance)
{	
	LPCWSTR subFolder = nullptr;

	// Get directory for user data. This will be kept separated from the
   // directory for the browser UI data.
	std::wstring userDataDirectory = GetAppDataDirectory();
	userDataDirectory.append(L"\\User Data");

	auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
	std::wstring m_language;
	if (!m_language.empty())
		options->put_Language(m_language.c_str());

	std::wstring additionalBrowserArguments;
	//additionalBrowserArguments = L"--auto-open-devtools-for-tabs";	
	if (!additionalBrowserArguments.empty())
		options->put_AdditionalBrowserArguments(additionalBrowserArguments.c_str());

	HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
		subFolder,
		userDataDirectory.c_str(),
		options.Get(),
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

std::wstring BrowserWindowEdge::s2ws(const std::string & str)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.from_bytes(str);
}

std::string BrowserWindowEdge::ws2s(const std::wstring & wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

BrowserWindowEdge::BrowserWindowEdge() : 
	NavigationCompletedEvent(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS))
{
	TRACE("\r\n\t+++ [%ld] %s - %p \r\n", GetCurrentThreadId(), __FUNCTION__, this);
}

BrowserWindowEdge::~BrowserWindowEdge()
{
	TRACE("\r\n\t--- [%ld] %s - %p \r\n", GetCurrentThreadId(), __FUNCTION__, this);
	if (m_webViewController)
	{
		m_webViewController->Close();
		m_webViewController = nullptr;
		m_webView = nullptr;
	}

	DestroyWindow();
}

void BrowserWindowEdge::RunAsync(std::function<void()> callback)
{
	auto* task = new std::function<void()>(callback);
	PostMessage(WM_APP, reinterpret_cast<WPARAM>(task), 0);
}

void BrowserWindowEdge::Init(std::wstring initialUri, std::function<void()> webviewCreatedCallback)
{	
	m_initialUri = initialUri;
	m_onWebViewFirstInitialized = webviewCreatedCallback;

	RunAsync([this] {
		InitWebView();
	});
}

void BrowserWindowEdge::Resize(RECT bounds)
{
	if (bounds.bottom == 0 && bounds.top == 0, bounds.left == 0, bounds.right == 0) {		
		GetClientRect(&bounds);
	}
		
	if (m_webViewController) {
		m_webViewController->put_Bounds(bounds);
		//m_webViewController->get_IsVisible(&isVisible);
	}	
}

HRESULT BrowserWindowEdge::Navigate(std::wstring url)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	ResetEvent(NavigationCompletedEvent.Get());
	SetEvent(NavigationCompletedEvent.Get());

	auto result = m_webView->Navigate(url.c_str());

	WaitForSingleObjectEx(NavigationCompletedEvent.Get(), INFINITE, FALSE);
	
	return result;
}

HRESULT BrowserWindowEdge::NavigateToString(std::wstring content)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	return m_webView->NavigateToString(content.c_str());
}

HRESULT BrowserWindowEdge::PostJson(web::json::value jsonObj)
{
	if (m_webView == nullptr)
		return ERROR_INVALID_HANDLE;

	utility::stringstream_t stream;
	jsonObj.serialize(stream);

	return m_webView->PostWebMessageAsJson(stream.str().c_str());
}

HRESULT BrowserWindowEdge::AddInitializeScript(std::wstring script, Microsoft::WRL::ComPtr<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler> handler)
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
				TRACE("\r\n\t+++ [%ld] %s - %p AddScriptToExecuteOnDocumentCreated failed Result %ld \r\n", GetCurrentThreadId(), __FUNCTION__, this, error);
			}
			TRACE("\r\n\t+++ [%ld] %s - %p AddScriptToExecuteOnDocumentCreated Id %s \r\n", GetCurrentThreadId(), __FUNCTION__, this, id);
			return error;
		});
	}

	return m_webView->AddScriptToExecuteOnDocumentCreated(script.c_str(), handler.Get());
}

HRESULT BrowserWindowEdge::InjectScript(std::wstring script, Microsoft::WRL::ComPtr <ICoreWebView2ExecuteScriptCompletedHandler> handler)
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
				TRACE("\r\n\t+++ [%ld] %s - %p ExecuteScript failed Result %ld \r\n", GetCurrentThreadId(), __FUNCTION__, this, error);
			}
			TRACE("\r\n\t+++ [%ld] %s - %p ExecuteScript Result %S \r\n", GetCurrentThreadId(), __FUNCTION__, this, result);
			return error;
		});
	}

	return m_webView->ExecuteScript(script.c_str(), handler.Get());

}

void BrowserWindowEdge::CheckFailure(HRESULT hr, LPCWSTR errorMessage)
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

std::wstring BrowserWindowEdge::GetAppDataDirectory()
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

std::wstring BrowserWindowEdge::GetFullPathFor(LPCWSTR relativePath)
{
	WCHAR path[MAX_PATH];
	GetModuleFileNameW(NULL, path, MAX_PATH);
	std::wstring pathName(path);

	std::size_t index = pathName.find_last_of(L"\\") + 1;
	pathName.replace(index, pathName.length(), relativePath);

	return pathName;
}

std::wstring BrowserWindowEdge::GetFilePathAsURI(std::wstring fullPath)
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

