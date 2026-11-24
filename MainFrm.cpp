// MainFrm.cpp : 메인 프레임 - 파일 명령, 제목, 상태 표시줄
#include "framework.h"
#include "resource.h"
#include "MainFrm.h"
#include "Theme.h"

static const TCHAR kOpenFilter[] =
    _T("모든 그림 파일|*.png;*.jpg;*.jpeg;*.jpe;*.jfif;*.bmp;*.dib;*.gif;*.tif;*.tiff;*.ico|")
    _T("PNG (*.png)|*.png|JPEG (*.jpg;*.jpeg;*.jpe;*.jfif)|*.jpg;*.jpeg;*.jpe;*.jfif|")
    _T("비트맵 (*.bmp;*.dib)|*.bmp;*.dib|GIF (*.gif)|*.gif|TIFF (*.tif;*.tiff)|*.tif;*.tiff|")
    _T("아이콘 (*.ico)|*.ico|모든 파일 (*.*)|*.*||");

static const TCHAR kSaveFilter[] =
    _T("PNG (*.png)|*.png|JPEG (*.jpg)|*.jpg|24비트 비트맵 (*.bmp)|*.bmp|GIF (*.gif)|*.gif|TIFF (*.tif)|*.tif||");
static const TCHAR* kSaveExts[] = { _T(".png"), _T(".jpg"), _T(".bmp"), _T(".gif"), _T(".tif") };

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_SETFOCUS()
    ON_WM_CLOSE()
    ON_WM_DROPFILES()
    ON_WM_GETMINMAXINFO()
    ON_COMMAND(ID_FILE_NEW, &CMainFrame::OnFileNew)
    ON_COMMAND(ID_FILE_OPEN, &CMainFrame::OnFileOpen)
    ON_COMMAND(ID_FILE_SAVE, &CMainFrame::OnFileSave)
    ON_COMMAND(ID_FILE_SAVE_AS, &CMainFrame::OnFileSaveAs)
    ON_UPDATE_COMMAND_UI_RANGE(ID_IND_POS, ID_IND_ZOOMBAR, &CMainFrame::OnUpdateIndicator)
    ON_COMMAND(ID_VIEW_DARKMODE, &CMainFrame::OnViewDarkMode)
    ON_UPDATE_COMMAND_UI(ID_VIEW_DARKMODE, &CMainFrame::OnUpdateViewDarkMode)
END_MESSAGE_MAP()

static UINT kIndicators[] = { ID_SEPARATOR, ID_IND_POS, ID_IND_SEL, ID_IND_SIZE, ID_IND_ZOOM, ID_IND_ZOOMBAR };

CMainFrame::CMainFrame()
{
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs))
        return FALSE;
    cs.style &= ~FWS_ADDTOTITLE;
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    HDC hdc = ::GetDC(nullptr);
    int dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(nullptr, hdc);
    cs.cx = MulDiv(1280, dpi, 96);
    cs.cy = MulDiv(860, dpi, 96);
    return TRUE;
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    // 다크 모드: 저장된 설정, 없으면 Windows 앱 모드를 따른다
    int dark = AfxGetApp()->GetProfileInt(_T("Settings"), _T("DarkMode"), -1);
    SetDarkMode(dark < 0 ? SystemPrefersDark() : dark != 0);

    if (!m_panel.Create(this, &m_canvas))
        return -1;

    if (!m_status.Create(this) || !m_status.SetIndicators(kIndicators, _countof(kIndicators)))
        return -1;
    HDC hdc = ::GetDC(nullptr);
    int dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(nullptr, hdc);
    m_status.SetPaneInfo(1, ID_IND_POS, SBPS_NORMAL, MulDiv(120, dpi, 96));
    m_status.SetPaneInfo(2, ID_IND_SEL, SBPS_NORMAL, MulDiv(120, dpi, 96));
    m_status.SetPaneInfo(3, ID_IND_SIZE, SBPS_NORMAL, MulDiv(130, dpi, 96));
    m_status.SetPaneInfo(4, ID_IND_ZOOM, SBPS_NORMAL, MulDiv(54, dpi, 96));
    m_status.SetPaneInfo(5, ID_IND_ZOOMBAR, SBPS_NOBORDERS, MulDiv(190, dpi, 96));
    m_status.SetBarStyle(m_status.GetBarStyle() & ~CBRS_BORDER_ANY);
    m_status.GetStatusBarCtrl().SetMinHeight(MulDiv(26, dpi, 96));
    m_status.OnZoomChanged = [this](double z) { m_canvas.SetZoomCentered(z); };
    m_status.OnZoomStep = [this](int dir) { m_canvas.ZoomStepCentered(dir); };

    if (!m_canvas.Create(this, AFX_IDW_PANE_FIRST))
        return -1;

    DragAcceptFiles(TRUE);
    ApplyTheme();
    UpdateTitle();
    return 0;
}

void CMainFrame::ApplyTheme()
{
    ApplyThemeToAppMenus();
    ApplyThemeToTitleBar(m_hWnd);
    m_canvas.ApplyTheme();
    m_panel.Invalidate(FALSE);
    m_status.Invalidate(FALSE);
    m_status.SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    DrawMenuBar();
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

// ---------------------------------------------------------------------------
// 다크 모드 메뉴 표시줄
// Win32 메뉴 표시줄은 다크 모드를 지원하지 않으므로, 문서화되지 않은 UAH 메시지
// (WM_UAHDRAWMENU / WM_UAHDRAWMENUITEM)를 처리해 직접 그린다.

#define WM_UAHDRAWMENU      0x0091
#define WM_UAHDRAWMENUITEM  0x0092

namespace
{
    struct UAHMENU { HMENU hmenu; HDC hdc; DWORD dwFlags; };
    union UAHMENUITEMMETRICS
    {
        struct { DWORD cx; DWORD cy; } rgsizeBar[2];
        struct { DWORD cx; DWORD cy; } rgsizePopup[4];
    };
    struct UAHMENUPOPUPMETRICS { DWORD rgcx[4]; DWORD fUpdateMaxWidths : 2; };
    struct UAHMENUITEM { int iPosition; UAHMENUITEMMETRICS umim; UAHMENUPOPUPMETRICS umpm; };
    struct UAHDRAWMENUITEM { DRAWITEMSTRUCT dis; UAHMENU um; UAHMENUITEM umi; };
}

bool CMainFrame::DrawDarkMenuBar(UINT message, LPARAM lParam)
{
    const ThemeColors& th = Theme();
    if (message == WM_UAHDRAWMENU)
    {
        UAHMENU* pUDM = (UAHMENU*)lParam;
        MENUBARINFO mbi = { sizeof(mbi) };
        if (!::GetMenuBarInfo(m_hWnd, OBJID_MENU, 0, &mbi))
            return false;
        CRect rw;
        GetWindowRect(&rw);
        CRect bar = mbi.rcBar;
        bar.OffsetRect(-rw.left, -rw.top);
        ::SetBkColor(pUDM->hdc, th.panelBg);
        ::ExtTextOut(pUDM->hdc, 0, 0, ETO_OPAQUE, &bar, nullptr, 0, nullptr);
        return true;
    }
    if (message == WM_UAHDRAWMENUITEM)
    {
        UAHDRAWMENUITEM* p = (UAHDRAWMENUITEM*)lParam;
        wchar_t text[256] = {};
        MENUITEMINFOW mii = { sizeof(mii), MIIM_STRING };
        mii.dwTypeData = text;
        mii.cch = _countof(text) - 1;
        ::GetMenuItemInfoW(p->um.hmenu, p->umi.iPosition, TRUE, &mii);

        UINT state = p->dis.itemState;
        COLORREF bg = th.panelBg;
        if (state & (ODS_HOTLIGHT | ODS_SELECTED))
            bg = (state & ODS_SELECTED) ? th.pressed : th.hover;
        COLORREF fg = (state & (ODS_INACTIVE | ODS_DISABLED | ODS_GRAYED)) ? th.textDim : th.text;
        ::SetBkColor(p->um.hdc, bg);
        ::ExtTextOut(p->um.hdc, 0, 0, ETO_OPAQUE, &p->dis.rcItem, nullptr, 0, nullptr);
        ::SetBkMode(p->um.hdc, TRANSPARENT);
        ::SetTextColor(p->um.hdc, fg);
        UINT fmt = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
        if (state & ODS_NOACCEL)
            fmt |= DT_HIDEPREFIX;
        ::DrawTextW(p->um.hdc, text, -1, &p->dis.rcItem, fmt);
        return true;
    }
    return false;
}

// 메뉴 표시줄 아래에 남는 1px 밝은 선을 덮는다
void CMainFrame::PaintMenuBarBottomLine()
{
    MENUBARINFO mbi = { sizeof(mbi) };
    if (!::GetMenuBarInfo(m_hWnd, OBJID_MENU, 0, &mbi))
        return;
    CRect rc, rw;
    GetClientRect(&rc);
    ClientToScreen(&rc);
    GetWindowRect(&rw);
    rc.OffsetRect(-rw.left, -rw.top);
    CRect line(rc.left, rc.top - 1, rc.right, rc.top);
    CWindowDC dc(this);
    dc.FillSolidRect(line, Theme().panelBg);
}

LRESULT CMainFrame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (IsDarkMode())
    {
        if ((message == WM_UAHDRAWMENU || message == WM_UAHDRAWMENUITEM) && DrawDarkMenuBar(message, lParam))
            return TRUE;
        if (message == WM_NCPAINT || message == WM_NCACTIVATE)
        {
            LRESULT r = CFrameWnd::WindowProc(message, wParam, lParam);
            PaintMenuBarBottomLine();
            return r;
        }
    }
    return CFrameWnd::WindowProc(message, wParam, lParam);
}

void CMainFrame::OnViewDarkMode()
{
    SetDarkMode(!IsDarkMode());
    AfxGetApp()->WriteProfileInt(_T("Settings"), _T("DarkMode"), IsDarkMode() ? 1 : 0);
    ApplyTheme();
}

void CMainFrame::OnUpdateViewDarkMode(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(IsDarkMode());
}

BOOL CMainFrame::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo)
{
    if (m_canvas.GetSafeHwnd() && m_canvas.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
        return TRUE;
    return CFrameWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
    // 텍스트 입력 중에는 단축키 대부분을 편집 컨트롤에 넘긴다
    if (pMsg->message == WM_KEYDOWN && m_canvas.GetSafeHwnd() && m_canvas.IsTextEditFocused())
    {
        bool ctrl = ::GetKeyState(VK_CONTROL) < 0;
        if (pMsg->wParam == VK_ESCAPE)
        {
            m_canvas.EndTextEdit(true);
            return TRUE;
        }
        if (ctrl)
        {
            switch (pMsg->wParam)
            {
            case 'B': SendMessage(WM_COMMAND, ID_TEXT_BOLD); return TRUE;
            case 'I': SendMessage(WM_COMMAND, ID_TEXT_ITALIC); return TRUE;
            case 'U': SendMessage(WM_COMMAND, ID_TEXT_UNDERLINE); return TRUE;
            case 'S': SendMessage(WM_COMMAND, ID_FILE_SAVE); return TRUE;
            }
        }
        return FALSE;
    }
    return CFrameWnd::PreTranslateMessage(pMsg);
}

void CMainFrame::OnSetFocus(CWnd*)
{
    if (m_canvas.GetSafeHwnd())
        m_canvas.FocusDefault();
}

void CMainFrame::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 640;
    lpMMI->ptMinTrackSize.y = 480;
    CFrameWnd::OnGetMinMaxInfo(lpMMI);
}

// ---------------------------------------------------------------------------

CString CMainFrame::DisplayName() const
{
    if (m_path.IsEmpty())
        return _T("제목 없음");
    int p = m_path.ReverseFind(_T('\\'));
    return p >= 0 ? m_path.Mid(p + 1) : m_path;
}

void CMainFrame::UpdateTitle()
{
    if (!GetSafeHwnd())
        return;
    CString title;
    bool modified = m_canvas.GetSafeHwnd() && m_canvas.IsModified();
    title.Format(_T("%s%s - 그림판"), modified ? _T("*") : _T(""), (LPCTSTR)DisplayName());
    SetWindowText(title);
}

void CMainFrame::SetStatusTexts(const CString& pos, const CString& sel, const CString& size, const CString& zoom,
                                double zoomValue)
{
    m_ind[0] = pos;
    m_ind[1] = sel;
    m_ind[2] = size;
    m_ind[3] = zoom;
    if (m_status.GetSafeHwnd())
    {
        for (int i = 0; i < 4; ++i)
            m_status.SetPaneText(i + 1, m_ind[i]);
        m_status.SetZoom(zoomValue);
    }
}

void CMainFrame::OnUpdateIndicator(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(TRUE);
    UINT i = pCmdUI->m_nID - ID_IND_POS;
    pCmdUI->SetText(i < 4 ? m_ind[i] : CString());
}

void CMainFrame::OnCanvasStateChanged()
{
    if (m_panel.GetSafeHwnd())
        m_panel.Refresh();
}

// ---------------------------------------------------------------------------
// 파일 명령

bool CMainFrame::PromptSave()
{
    if (!m_canvas.IsModified())
        return true;
    CString msg;
    msg.Format(_T("변경 내용을 %s에 저장하시겠습니까?"), (LPCTSTR)DisplayName());
    int r = MessageBox(msg, _T("그림판"), MB_YESNOCANCEL | MB_ICONWARNING);
    if (r == IDCANCEL)
        return false;
    if (r == IDYES)
        return DoSave(false);
    return true;
}

bool CMainFrame::DoSave(bool saveAs)
{
    CString path = m_path;
    if (saveAs || path.IsEmpty())
    {
        CString name = path.IsEmpty() ? CString(_T("제목 없음.png")) : DisplayName();
        CFileDialog dlg(FALSE, _T("png"), name, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST,
                        kSaveFilter, this);
        CString ext;
        int dot = name.ReverseFind(_T('.'));
        if (dot >= 0)
            ext = name.Mid(dot).MakeLower();
        for (int i = 0; i < _countof(kSaveExts); ++i)
            if (ext == kSaveExts[i])
                dlg.m_ofn.nFilterIndex = i + 1;
        if (ext == _T(".jpeg"))
            dlg.m_ofn.nFilterIndex = 2;
        if (dlg.DoModal() != IDOK)
            return false;
        path = dlg.GetPathName();
        // 확장자가 없으면 선택한 형식의 확장자를 붙인다
        int slash = path.ReverseFind(_T('\\'));
        if (path.ReverseFind(_T('.')) <= slash)
        {
            int fi = (int)dlg.m_ofn.nFilterIndex;
            path += kSaveExts[ClampV(fi, 1, (int)_countof(kSaveExts)) - 1];
        }
    }
    CString err;
    if (!m_canvas.SaveFile(path, err))
    {
        MessageBox(err, _T("그림판"), MB_ICONERROR);
        return false;
    }
    m_path = path;
    UpdateTitle();
    return true;
}

bool CMainFrame::OpenFile(LPCTSTR path)
{
    CString err;
    if (!m_canvas.LoadFile(path, err))
    {
        MessageBox(err, _T("그림판"), MB_ICONERROR);
        return false;
    }
    m_path = path;
    UpdateTitle();
    return true;
}

void CMainFrame::OnFileNew()
{
    if (!PromptSave())
        return;
    m_canvas.NewImage(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    m_path.Empty();
    UpdateTitle();
}

void CMainFrame::OnFileOpen()
{
    if (!PromptSave())
        return;
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY, kOpenFilter, this);
    if (dlg.DoModal() == IDOK)
        OpenFile(dlg.GetPathName());
}

void CMainFrame::OnFileSave()
{
    DoSave(false);
}

void CMainFrame::OnFileSaveAs()
{
    DoSave(true);
}

void CMainFrame::OnClose()
{
    if (m_canvas.GetSafeHwnd())
        m_canvas.FlattenPending();
    if (!PromptSave())
        return;
    CFrameWnd::OnClose();
}

void CMainFrame::OnDropFiles(HDROP hDropInfo)
{
    TCHAR path[MAX_PATH] = {};
    UINT n = ::DragQueryFile(hDropInfo, 0, path, MAX_PATH);
    ::DragFinish(hDropInfo);
    if (n == 0)
        return;
    if (!PromptSave())
        return;
    OpenFile(path);
}
