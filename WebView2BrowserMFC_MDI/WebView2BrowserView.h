
// WebView2BrowserView.h : interface of the CWebView2BrowserView class
//

#pragma once
#include "BrowserWindow.h"
#include "BrowserWindowEdge.h"

class CWebView2BrowserView : public CFormView
{	
	long m_Id;
	// browser with win32 native
	std::unique_ptr<BrowserWindow> browser;
	//browser with CWnd MFC Class
	std::unique_ptr<BrowserWindowEdge> browserEdge;

public: // create from serialization only
	CWebView2BrowserView() noexcept;
	DECLARE_DYNCREATE(CWebView2BrowserView)

public:
#ifdef AFX_DESIGN_TIME
	enum{ IDD = IDD_WEBVIEW2BROWSERMFC_MDI_FORM };
#endif

// Attributes
public:
	CWebView2BrowserDoc* GetDocument() const;

// Operations
public:

// Overrides
public:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual void OnInitialUpdate(); // called first time after construct

// Implementation
public:
	virtual ~CWebView2BrowserView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	afx_msg void OnFilePrintPreview();
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	DECLARE_MESSAGE_MAP()
public:
	CStatic m_Label_View;
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
//	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
	virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
};

#ifndef _DEBUG  // debug version in WebView2BrowserView.cpp
inline CWebView2BrowserDoc* CWebView2BrowserView::GetDocument() const
   { return reinterpret_cast<CWebView2BrowserDoc*>(m_pDocument); }
#endif

