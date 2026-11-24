// MainFrm.cpp : 메인 프레임 - 파일 명령, 제목, 상태 표시줄
#include "framework.h"
#include "resource.h"
#include "MainFrm.h"
#include "Theme.h"
#include "Dialogs.h"

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
    ON_COMMAND(ID_COLLAB_HOST, &CMainFrame::OnCollabHost)
    ON_COMMAND(ID_COLLAB_JOIN, &CMainFrame::OnCollabJoin)
    ON_COMMAND(ID_COLLAB_LEAVE, &CMainFrame::OnCollabLeave)
    ON_COMMAND(ID_COLLAB_COPYKEY, &CMainFrame::OnCollabCopyKey)
    ON_COMMAND(ID_VIEW_CHAT, &CMainFrame::OnViewChat)
    ON_UPDATE_COMMAND_UI_RANGE(ID_COLLAB_HOST, ID_VIEW_CHAT, &CMainFrame::OnUpdateCollab)
    ON_WM_TIMER()
    ON_MESSAGE(WM_APP_COLLAB_CLEANUP, &CMainFrame::OnCollabCleanupMsg)
    ON_MESSAGE(WM_APP_COLLAB_FAILED, &CMainFrame::OnCollabFailedMsg)
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

    // 공유 캔버스: 세션과 오른쪽 패널 (패널은 공유를 시작하면 나타난다)
    m_collab = std::make_unique<CCollabSession>(this);
    if (!m_chat.Create(this, m_collab.get()))
        return -1;
    m_chat.OnSend = [this](const CString& text)
    {
        m_collab->SendChat(text);
        for (auto& u : m_collab->Users())
            if (u.isMe)
                m_chat.AddMessage(u.id, u.color, text, false);
    };
    m_chat.OnCopyKey = [this]() { CopyInviteKey(); };
    m_chat.OnHide = [this]() { ShowChat(false); };
    SetTimer(1, 1000, nullptr);

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
    m_chat.ApplyTheme();
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
    // 채팅 입력 등 다른 편집 컨트롤에 입력 중이면 단축키(Del, Ctrl+C/V/Z 등)를 그 컨트롤에 넘긴다
    if (pMsg->message == WM_KEYDOWN)
    {
        HWND focus = ::GetFocus();
        TCHAR cls[32] = {};
        if (focus && ::GetClassName(focus, cls, _countof(cls)) && CString(cls).MakeLower().Find(_T("edit")) >= 0)
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
    if (m_collab && m_collab->IsActive())
        title += m_collab->GetRole() == CCollabSession::ROLE_HOST ? _T("  [공유 중 · 호스트]") : _T("  [공유 중 · 참가]");
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
    if (!ConfirmEndSession(_T("그림판을 닫으면")))
        return;
    if (!PromptSave())
        return;
    KillTimer(1);
    if (m_collab)
        m_collab->Stop();
    m_canvas.StopSync();
    CFrameWnd::OnClose();
}

// ---------------------------------------------------------------------------
// 공유 캔버스

void CMainFrame::ShowChat(bool show)
{
    if (!m_chat.GetSafeHwnd() || ((m_chat.GetStyle() & WS_VISIBLE) != 0) == show)
        return;
    m_chat.ShowWindow(show ? SW_SHOW : SW_HIDE);
    RecalcLayout();
    m_chat.Refresh();
}

void CMainFrame::CopyInviteKey()
{
    if (!m_collab || m_collab->InviteKey().IsEmpty() || !OpenClipboard())
        return;
    CString key = m_collab->InviteKey();
    ::EmptyClipboard();
    size_t bytes = (key.GetLength() + 1) * sizeof(WCHAR);
    HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (h)
    {
        memcpy(::GlobalLock(h), (LPCWSTR)key, bytes);
        ::GlobalUnlock(h);
        if (!::SetClipboardData(CF_UNICODETEXT, h))
            ::GlobalFree(h);
    }
    ::CloseClipboard();
}

// 공유 중이면 끝내도 되는지 묻는다
bool CMainFrame::ConfirmEndSession(LPCTSTR action)
{
    if (!m_collab || m_collab->GetRole() == CCollabSession::ROLE_NONE)
        return true;
    CString msg;
    if (m_collab->GetRole() == CCollabSession::ROLE_HOST && m_collab->Users().size() > 1)
        msg.Format(_T("%s 공유가 끝나고 참가자 %d명의 연결이 끊어집니다. 계속할까요?"), action,
                   (int)m_collab->Users().size() - 1);
    else
        msg.Format(_T("%s 공유가 끝납니다. 계속할까요?"), action);
    return MessageBox(msg, _T("그림판"), MB_OKCANCEL | MB_ICONQUESTION) == IDOK;
}

void CMainFrame::OnCollabHost()
{
    if (!ConfirmEndSession(_T("새로 공유를 시작하면 지금")))
        return;
    CCollabHostDlg dlg(this);
    if (dlg.DoModal() != IDOK)
        return;
    m_canvas.StopSync();
    m_canvas.FlattenPending();
    CString err;
    if (!m_collab->StartHost(dlg.m_id, dlg.m_ip, dlg.m_port, err))
    {
        MessageBox(err, _T("그림판"), MB_ICONERROR);
        return;
    }
    m_canvas.StartSync();
    CopyInviteKey();
    ShowChat(true);
    m_chat.AddMessage(CString(), 0, _T("공유를 시작했습니다. 초대 키를 클립보드에 복사했습니다. 참가자에게 알려 주세요."), true);
    if (dlg.m_ip != 0x7F000001)
        m_chat.AddMessage(CString(), 0, _T("다른 PC가 접속하지 못하면 Windows 방화벽에서 이 프로그램을 허용해 주세요."), true);
    UpdateTitle();
}

void CMainFrame::OnCollabJoin()
{
    if (!ConfirmEndSession(_T("다른 캔버스에 참가하면 지금")))
        return;
    CCollabJoinDlg dlg(this);
    if (dlg.DoModal() != IDOK)
        return;
    m_canvas.FlattenPending();
    if (!PromptSave())      // 참가하면 내 그림이 호스트의 그림으로 바뀐다
        return;
    m_canvas.StopSync();
    CString err;
    if (!m_collab->Join(dlg.m_key, dlg.m_id, err))
    {
        MessageBox(err, _T("그림판"), MB_ICONERROR);
        return;
    }
    ShowChat(true);
    m_chat.AddMessage(CString(), 0, _T("호스트에 연결하는 중입니다..."), true);
}

void CMainFrame::OnCollabLeave()
{
    if (!m_collab || m_collab->GetRole() == CCollabSession::ROLE_NONE)
        return;
    if (m_collab->GetRole() == CCollabSession::ROLE_HOST && m_collab->Users().size() > 1 &&
        !ConfirmEndSession(_T("지금")))
        return;
    bool host = m_collab->GetRole() == CCollabSession::ROLE_HOST;
    m_collab->Stop();
    m_canvas.StopSync();
    m_chat.AddMessage(CString(), 0, host ? _T("공유를 끝냈습니다.") : _T("공유 캔버스에서 나왔습니다."), true);
    UpdateTitle();
}

void CMainFrame::OnCollabCopyKey()
{
    CopyInviteKey();
}

void CMainFrame::OnViewChat()
{
    ShowChat((m_chat.GetStyle() & WS_VISIBLE) == 0);
}

void CMainFrame::OnUpdateCollab(CCmdUI* pCmdUI)
{
    CCollabSession::Role role = m_collab ? m_collab->GetRole() : CCollabSession::ROLE_NONE;
    switch (pCmdUI->m_nID)
    {
    case ID_COLLAB_LEAVE:   pCmdUI->Enable(role != CCollabSession::ROLE_NONE); break;
    case ID_COLLAB_COPYKEY: pCmdUI->Enable(role == CCollabSession::ROLE_HOST); break;
    case ID_VIEW_CHAT:      pCmdUI->SetCheck(m_chat.GetSafeHwnd() && (m_chat.GetStyle() & WS_VISIBLE)); break;
    default:                pCmdUI->Enable(TRUE); break;
    }
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1 && m_collab)
    {
        m_collab->Tick();
        return;
    }
    CFrameWnd::OnTimer(nIDEvent);
}

LRESULT CMainFrame::OnCollabCleanupMsg(WPARAM, LPARAM)
{
    if (m_collab)
        m_collab->Cleanup();
    return 0;
}

LRESULT CMainFrame::OnCollabFailedMsg(WPARAM, LPARAM)
{
    // 소켓 알림 처리 중에 대화 상자를 띄우지 않도록 메시지로 미뤄서 보여 준다
    CString msg = m_failMessage;
    m_failMessage.Empty();
    if (!msg.IsEmpty())
        MessageBox(msg, _T("그림판 - 공유 캔버스"), MB_ICONWARNING);
    return 0;
}

// ---- ICollabEvents

void CMainFrame::OnCollabStateChanged()
{
    m_chat.Refresh();
    UpdateTitle();
    m_canvas.Invalidate(FALSE);
}

void CMainFrame::OnCollabChat(const CString& id, COLORREF color, const CString& text, bool system)
{
    m_chat.AddMessage(id, color, text, system);
    if (!system)
        ShowChat(true);
}

void CMainFrame::OnCollabEdit(const CRect& rc, const std::vector<DWORD>& px)
{
    m_canvas.ApplyRemoteRegion(rc, px);
}

void CMainFrame::OnCollabFull(int w, int h, const std::vector<DWORD>& px)
{
    m_canvas.ApplyRemoteFull(w, h, px);
}

void CMainFrame::OnCollabCursor(CPoint oldPos, bool oldVisible, CPoint pos, bool visible)
{
    if (oldVisible)
        m_canvas.InvalidateRemoteCursor(oldPos);
    if (visible)
        m_canvas.InvalidateRemoteCursor(pos);
}

void CMainFrame::OnCollabJoined()
{
    m_canvas.StartSync();
    m_path.Empty();
    m_canvas.SetModified(false);
    CString host;
    for (auto& u : m_collab->Users())
        if (u.isHost)
            host = u.id;
    m_chat.AddMessage(CString(), 0, host + _T(" 님의 캔버스에 참가했습니다."), true);
    UpdateTitle();
}

void CMainFrame::OnCollabFailed(const CString& message)
{
    m_canvas.StopSync();
    m_chat.AddMessage(CString(), 0, message, true);
    m_chat.Refresh();
    UpdateTitle();
    m_failMessage = message;
    PostMessage(WM_APP_COLLAB_FAILED);
}

void CMainFrame::CollabFlushPending()
{
    m_canvas.FlushSync();
}

const CBitmap32& CMainFrame::CollabCurrentImage()
{
    return m_canvas.Image();
}

void CMainFrame::CollabRequestCleanup()
{
    if (GetSafeHwnd())
        PostMessage(WM_APP_COLLAB_CLEANUP);
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
