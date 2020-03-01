#include "pch.h"
#include "framework.h"
#include "BrowserWindow.h"

using namespace Microsoft::WRL;

wil::com_ptr<ICoreWebView2Environment> BrowserWindow::m_webViewEnvironment = nullptr;


void BrowserWindow::SetBrokers()
{
	// Set the message broker for the webview. This will capture messages from web content.
	m_MessageBroker = Callback<ICoreWebView2WebMessageReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* eventArgs) -> HRESULT
	{
		wil::unique_cotaskmem_string jsonString;
		CheckFailure(eventArgs->get_WebMessageAsJson(&jsonString), L"");  // Get the message from the UI WebView as JSON formatted string
		web::json::value jsonObj = web::json::value::parse(jsonString.get());

		if (!jsonObj.has_field(L"message"))
		{
			OutputDebugString(L"No message code provided\n");
			return S_OK;
		}

		if (!jsonObj.has_field(L"args"))
		{
			OutputDebugString(L"The message has no args field\n");
			return S_OK;
		}

		TRACE("[%s] %S", __FUNCTION__, jsonString.get());

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
					OutputDebugString(L"Requested unknown browser page\n");
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
			OutputDebugString(L"Unexpected message\n");
		}
		break;
		}

		return S_OK;
	});

	m_NavigationStartingBroker = Callback<ICoreWebView2NavigationStartingEventHandler>(
		[](ICoreWebView2* webview, ICoreWebView2NavigationStartingEventArgs * args) -> HRESULT {
		wil::unique_cotaskmem_string uri;
		args->get_Uri(&uri);
		std::wstring source(uri.get());
		if (source.substr(0, 5) != L"https") {
			args->put_Cancel(true);
		}
		return S_OK;
	});

	m_devToolsSecurityBroker = Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args) -> HRESULT
	{
		wil::unique_cotaskmem_string parameterObjectAsJson;
		RETURN_IF_FAILED(args->get_ParameterObjectAsJson(&parameterObjectAsJson));
		TRACE("\r\nm_devToolsSecurityBroker %S\r\n", parameterObjectAsJson.get());

		return S_OK;
	}).Get();

	m_devToolsLogBroker = Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
		[this](ICoreWebView2* webview, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* args) -> HRESULT
	{
		wil::unique_cotaskmem_string parameterObjectAsJson;
		RETURN_IF_FAILED(args->get_ParameterObjectAsJson(&parameterObjectAsJson));
		TRACE("\r\nm_devToolsLogBroker %S\r\n", parameterObjectAsJson.get());

		return S_OK;
	}).Get();

}

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

HRESULT BrowserWindow::Navigate(std::wstring url)
{
	return m_webView->Navigate(url.c_str());;
}

HRESULT BrowserWindow::PostJson(web::json::value jsonObj)
{
	utility::stringstream_t stream;
	jsonObj.serialize(stream);

	return m_webView->PostWebMessageAsJson(stream.str().c_str());
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
		Navigate(url.c_str());

		// Register events	
		SetBrokers();
		RETURN_IF_FAILED(m_webView->add_WebMessageReceived(m_MessageBroker.Get(), &m_EventRegistrationToken));
		RETURN_IF_FAILED(m_webView->add_NavigationStarting(m_NavigationStartingBroker.Get(), &m_EventRegistrationToken));

		// Enable listening for security events to update secure icon
		wil::com_ptr<ICoreWebView2DevToolsProtocolEventReceiver> devToolsProtocolEventReceiver;

		RETURN_IF_FAILED(m_webView->CallDevToolsProtocolMethod(L"Security.enable", L"{}", nullptr));
		BrowserWindow::CheckFailure(m_webView->GetDevToolsProtocolEventReceiver(L"Security.securityStateChanged", &devToolsProtocolEventReceiver), L"");
		devToolsProtocolEventReceiver->add_DevToolsProtocolEventReceived(m_devToolsSecurityBroker.Get(), &m_EventRegistrationToken);

		RETURN_IF_FAILED(m_webView->CallDevToolsProtocolMethod(L"Log.enable", L"{}", nullptr));
		BrowserWindow::CheckFailure(m_webView->GetDevToolsProtocolEventReceiver(L"Log.entryAdded", &devToolsProtocolEventReceiver), L"");
		devToolsProtocolEventReceiver->add_DevToolsProtocolEventReceived(m_devToolsLogBroker.Get(), &m_EventRegistrationToken);		

		return S_OK;
	}).Get());

	return false;
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

		MessageBoxW(nullptr, message.c_str(), nullptr, MB_OK);
	}
}

std::wstring BrowserWindow::GetAppDataDirectory()
{
	std::wstring s_title = _T("WebView2Browser"); //pegar depois do aplication
	TCHAR path[MAX_PATH];
	std::wstring dataDirectory;
	HRESULT hr = SHGetFolderPath(nullptr, CSIDL_APPDATA, NULL, 0, path);
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
	GetModuleFileNameW(m_hInst, path, MAX_PATH);
	std::wstring pathName(path);

	std::size_t index = pathName.find_last_of(L"\\") + 1;
	pathName.replace(index, pathName.length(), relativePath);

	return pathName;
}

