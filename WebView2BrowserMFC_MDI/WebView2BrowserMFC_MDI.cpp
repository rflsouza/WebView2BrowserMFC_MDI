
// WebView2BrowserMFC_MDI.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "afxwinappex.h"
#include "afxdialogex.h"
#include "WebView2BrowserMFC_MDI.h"
#include "MainFrm.h"
#include <AFXPRIV.H>

#include "WebView2BrowserDoc.h"
#include "WebView2BrowserView.h"

#include "BrowserWindow.h"
#include "BrowserWindowEdge.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

std::atomic<long> g_views = 0;
std::atomic<long> g_browsers = 0;

// CWebView2BrowserApp

BEGIN_MESSAGE_MAP(CWebView2BrowserApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CWebView2BrowserApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
	ON_COMMAND(ID_FILE_SWITCHVIEW, &CWebView2BrowserApp::OnFileSwitchView)
END_MESSAGE_MAP()


// CWebView2BrowserApp construction

CWebView2BrowserApp::CWebView2BrowserApp() noexcept
{
	m_bHiColorIcons = TRUE;

	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_ALL_ASPECTS;
#ifdef _MANAGED
	// If the application is built using Common Language Runtime support (/clr):
	//     1) This additional setting is needed for Restart Manager support to work properly.
	//     2) In your project, you must add a reference to System.Windows.Forms in order to build.
	System::Windows::Forms::Application::SetUnhandledExceptionMode(System::Windows::Forms::UnhandledExceptionMode::ThrowException);
#endif

	// TODO: replace application ID string below with unique ID string; recommended
	// format for string is CompanyName.ProductName.SubProduct.VersionInformation
	SetAppID(_T("WebView2BrowserMFCMDI.AppID.NoVersion"));

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// The one and only CWebView2BrowserApp object

CWebView2BrowserApp theApp;


// CWebView2BrowserApp initialization

BOOL CWebView2BrowserApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();


	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	EnableTaskbarInteraction(FALSE);

	// AfxInitRichEdit2() is required to use RichEdit control
	// AfxInitRichEdit2();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));
	LoadStdProfileSettings(0);  // Load standard INI file options (including MRU)

	BrowserWindow::InitInstance(theApp.m_hInstance);
	BrowserWindowEdge::InitInstance(theApp.m_hInstance);

	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CWebView2BrowserDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CWebView2BrowserView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);


	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);



	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	//  resize screen.	
	int screen_x_size = GetSystemMetrics(SM_CXSCREEN), screen_y_size = GetSystemMetrics(SM_CYSCREEN);
	int x_5percent = screen_x_size * 0.1, y_5percent = screen_y_size * 0.1;
	m_pMainWnd->SetWindowPos(m_pActiveWnd, x_5percent, y_5percent, screen_x_size - x_5percent * 2, screen_y_size - y_5percent * 2, SWP_NOZORDER);

	{
		//https://docs.microsoft.com/en-us/cpp/mfc/adding-multiple-views-to-a-single-document?redirectedfrom=MSDN&view=vs-2019#vcconswitchingfunctiona4
		CView *pActiveView = ((CFrameWnd *)m_pMainWnd)->GetActiveView();
		m_pOldView = pActiveView;
		m_pNewView = (CView *) new CWebView2BrowserView;
		if (NULL == m_pNewView)
			return FALSE;

		CDocument *pCurrentDoc = ((CFrameWnd *)m_pMainWnd)->GetActiveDocument();

		// Initialize a CCreateContext to point to the active document.
		// With this context, the new view is added to the document
		// when the view is created in CView::OnCreate().
		CCreateContext newContext;
		newContext.m_pNewViewClass = NULL;
		newContext.m_pNewDocTemplate = NULL;
		newContext.m_pLastView = NULL;
		newContext.m_pCurrentFrame = NULL;
		newContext.m_pCurrentDoc = pCurrentDoc;

		// The ID of the initial active view is AFX_IDW_PANE_FIRST.
		// Incrementing this value by one for additional views works
		// in the standard document/view case but the technique cannot
		// be extended for the CSplitterWnd case.
		UINT viewID = AFX_IDW_PANE_FIRST + 1;
		CRect rect(0, 0, 0, 0); // Gets resized later.		
		GetClientRect(((CFrameWnd *)m_pMainWnd)->m_hWnd, &rect);

		// Create the new view. In this example, the view persists for
		// the life of the application. The application automatically
		// deletes the view when the application is closed.
		m_pNewView->Create(NULL, _T("AnyWindowName"), WS_CHILD, rect, m_pMainWnd, viewID, &newContext);

		// When a document template creates a view, the WM_INITIALUPDATE
		// message is sent automatically. However, this code must
		// explicitly send the message, as follows.
		m_pNewView->SendMessage(WM_INITIALUPDATE, 0, 0);
	}

	// The one and only window has been initialized, so show and update it
	m_pMainWnd->ShowWindow(SW_SHOW);
	
	//  resize screen.	
	m_pMainWnd->SetWindowPos(m_pActiveWnd, x_5percent, y_5percent, screen_x_size - x_5percent * 2, screen_y_size - y_5percent * 2, SWP_NOZORDER);

	m_pMainWnd->UpdateWindow();
	return TRUE;
}

int CWebView2BrowserApp::ExitInstance()
{
	//TODO: handle additional resources you may have added
	AfxOleTerm(FALSE);

	return CWinAppEx::ExitInstance();
}

// CWebView2BrowserApp message handlers


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg() noexcept;

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() noexcept : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// App command to run the dialog
void CWebView2BrowserApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CWebView2BrowserApp customization load/save methods

void CWebView2BrowserApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
}

void CWebView2BrowserApp::LoadCustomState()
{
}

void CWebView2BrowserApp::SaveCustomState()
{
}

// CWebView2BrowserApp message handlers

void CWebView2BrowserApp::OnFileSwitchView()
{
	// TODO: Add your command handler code here		
	// MessageBox(m_pMainWnd->GetSafeHwnd(), L"fecha", L"", 0);
	
	SwitchView();
}


CView *CWebView2BrowserApp::SwitchView()
{
	CView *pActiveView = ((CFrameWnd *)m_pMainWnd)->GetActiveView();

	CView *pNewView = NULL;
	if (pActiveView == m_pOldView)
		pNewView = m_pNewView;
	else
		pNewView = m_pOldView;

	// Exchange view window IDs so RecalcLayout() works.
#ifndef _WIN32
	UINT temp = ::GetWindowWord(pActiveView->m_hWnd, GWW_ID);
	::SetWindowWord(pActiveView->m_hWnd, GWW_ID, ::GetWindowWord(pNewView->m_hWnd, GWW_ID));
	::SetWindowWord(pNewView->m_hWnd, GWW_ID, temp);
#else
	UINT temp = ::GetWindowLong(pActiveView->m_hWnd, GWL_ID);
	::SetWindowLong(pActiveView->m_hWnd, GWL_ID, ::GetWindowLong(pNewView->m_hWnd, GWL_ID));
	::SetWindowLong(pNewView->m_hWnd, GWL_ID, temp);
#endif

	pActiveView->ShowWindow(SW_HIDE);
	pNewView->ShowWindow(SW_SHOW);
	((CFrameWnd *)m_pMainWnd)->SetActiveView(pNewView);
	((CFrameWnd *)m_pMainWnd)->RecalcLayout();
	pNewView->Invalidate();
	return pActiveView;
}