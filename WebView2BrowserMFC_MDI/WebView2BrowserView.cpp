
// WebView2BrowserView.cpp : implementation of the CWebView2BrowserView class
//

#include "pch.h"
#include "framework.h"
// SHARED_HANDLERS can be defined in an ATL project implementing preview, thumbnail
// and search filter handlers and allows sharing of document code with that project.
#ifndef SHARED_HANDLERS
#include "WebView2BrowserMFC_MDI.h"
#endif

#include "WebView2BrowserDoc.h"
#include "WebView2BrowserView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CWebView2BrowserView

IMPLEMENT_DYNCREATE(CWebView2BrowserView, CFormView)

BEGIN_MESSAGE_MAP(CWebView2BrowserView, CFormView)
	ON_WM_CONTEXTMENU()
	ON_WM_RBUTTONUP()
	ON_WM_SIZE()
END_MESSAGE_MAP()

// CWebView2BrowserView construction/destruction

CWebView2BrowserView::CWebView2BrowserView() noexcept
	: CFormView(IDD_WEBVIEW2BROWSERMFC_MDI_FORM)
{	
	// TODO: add construction code here	
	m_Id = ++g_views;	
	TRACE("\r\n\t++++ [%ld] %s - %p id:%ld\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id);
}

CWebView2BrowserView::~CWebView2BrowserView()
{	
	TRACE("\r\n\t---- [%ld] %s - %p id:%ld\r\n", GetCurrentThreadId(), __FUNCTION__, this, m_Id);
}

void CWebView2BrowserView::DoDataExchange(CDataExchange* pDX)
{
	CFormView::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_Label_View, m_Label_View);
}

BOOL CWebView2BrowserView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CFormView::PreCreateWindow(cs);
}

void CWebView2BrowserView::OnInitialUpdate()
{
	CFormView::OnInitialUpdate();
	GetParentFrame()->RecalcLayout();
	ResizeParentToFit();
	
	std::wstring view = L"View " + std::to_wstring(m_Id);
	m_Label_View.SetWindowText(view.c_str());
	
	//// Try get startup Browser Completed
	//std::mutex mutex_;
	//std::condition_variable condVar;
	//std::unique_lock<std::mutex> lck(mutex_);	
	//Microsoft::WRL::Wrappers::Event startupBrowserCompleted(CreateEventEx(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, WRITE_OWNER | EVENT_ALL_ACCESS));

	browser = std::make_unique<BrowserWindow>(theApp.m_hInstance, GetSafeHwnd(),
		BrowserWindow::GetFilePathAsURI(BrowserWindow::GetFullPathFor(L"htmls\\default.htm")),
		[&] {		
		if (browser) {
			//browser->Navigate(L"https://www.uol.com.br");
			//browser->Navigate(BrowserWindow::GetFilePathAsURI(BrowserWindow::GetFullPathFor(L"htmls\\default.htm")));
			//browser->AddInitializeScript(L"alert('oi');");

			std::wstring getTitleScript(
				// Look for a title tag
				L"(() => {"
				L"    const titleTag = document.getElementsByTagName('title')[0];"
				L"    if (titleTag) {"
				L"        return titleTag.innerHTML;"
				L"    }"
				// No title tag, look for the file name
				L"    pathname = window.location.pathname;"
				L"    var filename = pathname.split('/').pop();"
				L"    if (filename) {"
				L"        return filename;"
				L"    }"
				// No file name, look for the hostname
				L"    const hostname =  window.location.hostname;"
				L"    if (hostname) {"
				L"        return hostname;"
				L"    }"
				// Fallback: let the UI use a generic title
				L"    return '';"
				L"})();"
			);
			
			BrowserWindow::CheckFailure(browser->InjectScript(getTitleScript.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[this](HRESULT error, PCWSTR result) -> HRESULT
			{
				RETURN_IF_FAILED(error);

				web::json::value jsonObj = web::json::value::parse(result);
				
				TRACE("\r\nExecuteScriptCompleted Result: %S\r\n", result);
				return S_OK;
			})), L"Can't update title.");


			//web::json::value jsonObj = web::json::value::parse(L"{\"SetColor\":\"blue\"}");									
			//browser->PostJson(jsonObj);

			web::json::value jsonObj = web::json::value::parse(L"{}");
			jsonObj[L"message"] = web::json::value(2);
			jsonObj[L"args"] = web::json::value::parse(L"{}");
			jsonObj[L"args"][L"interaction"] = web::json::value::parse(L"{}");
			jsonObj[L"args"][L"interaction"][L"id"] = web::json::value::number(1);
			jsonObj[L"args"][L"interaction"][L"string"] = web::json::value::string(L"ação é agora");

			BrowserWindow::CheckFailure(browser->PostJson(jsonObj),L"Can't send Init.");
			
			std::wstring initializationScript(
					// Look for a title tag
					//L"(() => {"
					L"    let titleTag = 'teste Rafael';"
					L"    initialization(titleTag);"	
					L"    return 'OK';"
					//L"})();"
				);

			BrowserWindow::CheckFailure(browser->InjectScript(initializationScript.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
				[this](HRESULT error, PCWSTR result) -> HRESULT
			{
				RETURN_IF_FAILED(error);

				web::json::value jsonObj = web::json::value::parse(result);

				TRACE("\r\nExecuteScriptCompleted initializationScript Result: %S\r\n", result);
				return S_OK;
			})), L"Can't update title.");
			
		}
		
		
		// Set the completion event and return.
		//condVar.notify_one();
		//SetEvent(startupBrowserCompleted.Get());
	});
		
	//std::cout << "Waiting " << std::endl;
	//condVar.wait(lck);
	// Wait for the timer to complete.
	//WaitForSingleObjectEx(startupBrowserCompleted.Get(), INFINITE, FALSE);
	//std::cout << "Running " << std::endl;

	//std::async(std::launch::async,
	//	[this] {
	//	std::this_thread::sleep_for(std::chrono::seconds(10));
	//	std::wstring url = BrowserWindow::GetFilePathAsURI(BrowserWindow::GetFullPathFor(L"htmls\\WebMessage.html"));
	//	browser->Navigate(url);
	//});	
}

void CWebView2BrowserView::OnRButtonUp(UINT /* nFlags */, CPoint point)
{
	ClientToScreen(&point);
	OnContextMenu(this, point);
}

void CWebView2BrowserView::OnContextMenu(CWnd* /* pWnd */, CPoint point)
{
#ifndef SHARED_HANDLERS
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
#endif
}


// CWebView2BrowserView diagnostics

#ifdef _DEBUG
void CWebView2BrowserView::AssertValid() const
{
	CFormView::AssertValid();
}

void CWebView2BrowserView::Dump(CDumpContext& dc) const
{
	CFormView::Dump(dc);
}

CWebView2BrowserDoc* CWebView2BrowserView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CWebView2BrowserDoc)));
	return (CWebView2BrowserDoc*)m_pDocument;
}
#endif //_DEBUG


// CWebView2BrowserView message handlers

void CWebView2BrowserView::OnSize(UINT nType, int cx, int cy)
{
	// TODO: Add your message handler code here
	//TRACE("%s Type:%ld x:%ld y:%ld\r\n", __FUNCTION__, nType, cx, cy);

	CFormView::OnSize(nType, cx, cy);
	if (browser != nullptr) {
		//CRect rect; // Gets resized later.		
		//GetClientRect(&rect);
		//TRACE("%s right:%ld bottom:%ld\r\n", __FUNCTION__, rect.right, rect.bottom);
		//GetParentFrame()->GetClientRect(&rect);
		//TRACE("%s Type:%ld x:%ld y:%ld\r\n", __FUNCTION__, nType, cx, cy);
		//browser->Resize(rect);

		browser->Resize();
	}
		//GetClientRect(((CFrameWnd *)m_pMainWnd)->m_hWnd, &rect);
	//TRACE("%s Type:%ld x:%ld y:%ld\r\n", __FUNCTION__, nType, cx, cy);
}


LRESULT CWebView2BrowserView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{	
	// TODO: Add your specialized code here and/or call the base class
	return CFormView::WindowProc(message, wParam, lParam);
}
