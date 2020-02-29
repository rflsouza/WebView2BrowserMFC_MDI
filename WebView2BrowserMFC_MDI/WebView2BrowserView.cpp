
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
	
	browser = std::make_unique<BrowserWindow>(GetSafeHwnd());
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
	CFormView::OnSize(nType, cx, cy);
	if (browser != nullptr) {
		browser->Resize();		
	}		
	//TRACE("%s Type:%ld x:%ld y:%ld\r\n", __FUNCTION__, nType, cx, cy);
}
