// PaintApp.cpp : 애플리케이션 진입점
#include "framework.h"
#include "resource.h"
#include "MainFrm.h"

class CPaintApp : public CWinApp
{
public:
    BOOL InitInstance() override;
    int ExitInstance() override;

protected:
    afx_msg void OnAppAbout();
    DECLARE_MESSAGE_MAP()

private:
    ULONG_PTR m_gdiplusToken = 0;
};

CPaintApp theApp;

BEGIN_MESSAGE_MAP(CPaintApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, &CPaintApp::OnAppAbout)
    ON_COMMAND(ID_APP_EXIT, &CWinApp::OnAppExit)
END_MESSAGE_MAP()

BOOL CPaintApp::InitInstance()
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    ::InitCommonControlsEx(&icc);

    CWinApp::InitInstance();

    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gsi, nullptr);

    SetRegistryKey(_T("SwPaint"));

    CMainFrame* pFrame = new CMainFrame;
    m_pMainWnd = pFrame;
    if (!pFrame->LoadFrame(IDR_MAINFRAME, WS_OVERLAPPEDWINDOW, nullptr, nullptr))
        return FALSE;

    pFrame->ShowWindow(m_nCmdShow);
    pFrame->UpdateWindow();

    // 창 배치가 끝난 뒤에 열어야 '창에 맞추기' 배율이 올바르게 계산된다
    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);
    if (!cmdInfo.m_strFileName.IsEmpty())
        pFrame->OpenFile(cmdInfo.m_strFileName);
    return TRUE;
}

int CPaintApp::ExitInstance()
{
    if (m_gdiplusToken)
        Gdiplus::GdiplusShutdown(m_gdiplusToken);
    return CWinApp::ExitInstance();
}

void CPaintApp::OnAppAbout()
{
    AfxMessageBox(_T("그림판 (MFC)\n\nWindows 11 그림판과 비슷한 기능을 제공하는 MFC 기반 그림 편집기입니다."),
                  MB_ICONINFORMATION);
}
