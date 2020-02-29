
// WebView2BrowserMFC_MDI.h : main header file for the WebView2BrowserMFC_MDI application
//
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols


// CWebView2BrowserApp:
// See WebView2BrowserMFC_MDI.cpp for the implementation of this class
//

class CWebView2BrowserApp : public CWinAppEx
{
public:
	CWebView2BrowserApp() noexcept;


// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation
	UINT  m_nAppLook;
	BOOL  m_bHiColorIcons;

	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();

	CView *m_pOldView;
	CView *m_pNewView;
	CView *SwitchView();

	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
	afx_msg void OnFileSwitchView();
};

extern CWebView2BrowserApp theApp;
