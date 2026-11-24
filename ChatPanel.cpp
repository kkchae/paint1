// ChatPanel.cpp : 오른쪽 공유 패널 (상태, 초대 키, 참가자, 채팅)
#include "framework.h"
#include "resource.h"
#include "ChatPanel.h"
#include "Collab.h"
#include "Theme.h"

static const UINT_PTR TIMER_COPIED = 1;
static const int MAX_VISIBLE_USERS = 8;
static const size_t MAX_LINES = 500;

BEGIN_MESSAGE_MAP(CChatPanel, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_EN_SETFOCUS(IDC_CHAT_INPUT, &CChatPanel::OnInputFocus)
    ON_EN_KILLFOCUS(IDC_CHAT_INPUT, &CChatPanel::OnInputFocus)
    ON_MESSAGE(WM_SIZEPARENT, &CChatPanel::OnSizeParent)
END_MESSAGE_MAP()

void CChatPanel::OnInputFocus()
{
    InvalidateRect(m_inputBox, FALSE);
}

static void MakeFont(CFont& f, int dpi, int pt, int weight, LPCTSTR face)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(pt, dpi, 72);
    lf.lfWeight = weight;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, face);
    f.CreateFontIndirect(&lf);
}

BOOL CChatPanel::Create(CWnd* pParent, CCollabSession* session)
{
    m_session = session;
    HDC hdc = ::GetDC(nullptr);
    m_dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(nullptr, hdc);
    MakeFont(m_font, m_dpi, 9, FW_NORMAL, _T("맑은 고딕"));
    MakeFont(m_bold, m_dpi, 9, FW_BOLD, _T("맑은 고딕"));
    MakeFont(m_title, m_dpi, 12, FW_BOLD, _T("맑은 고딕"));
    MakeFont(m_mono, m_dpi, 8, FW_SEMIBOLD, _T("Consolas"));

    CString cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!CWnd::CreateEx(0, cls, _T(""), WS_CHILD | WS_CLIPCHILDREN,
                        CRect(0, 0, PanelWidth(), 100), pParent, AFX_IDW_CONTROLBAR_FIRST + 41))
        return FALSE;

    if (!m_log.Create(WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                      CRect(0, 0, 10, 10), this, IDC_CHAT_LOG))
        return FALSE;
    m_log.SetEventMask(0);
    m_log.SetTargetDevice(nullptr, 0);      // 창 너비에 맞춰 줄 바꿈
    m_log.SetFont(&m_font);

    if (!m_input.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, CRect(0, 0, 10, 10), this, IDC_CHAT_INPUT))
        return FALSE;
    m_input.SetFont(&m_font);
    m_input.SetCueBanner(L"메시지를 입력하고 Enter");
    m_input.LimitText(1000);

    ApplyTheme();
    Layout();
    return TRUE;
}

LRESULT CChatPanel::OnSizeParent(WPARAM, LPARAM lParam)
{
    if (!(GetStyle() & WS_VISIBLE))
        return 0;
    AFX_SIZEPARENTPARAMS* p = (AFX_SIZEPARENTPARAMS*)lParam;
    int w = min(PanelWidth(), (int)(p->rect.right - p->rect.left) / 2);
    CRect rc(p->rect.right - w, p->rect.top, p->rect.right, p->rect.bottom);
    p->rect.right -= w;
    p->sizeTotal.cx += w;
    if (p->hDWP)
        p->hDWP = ::DeferWindowPos(p->hDWP, m_hWnd, nullptr, rc.left, rc.top, rc.Width(), rc.Height(),
                                   SWP_NOACTIVATE | SWP_NOZORDER);
    return 0;
}

void CChatPanel::Refresh()
{
    if (!GetSafeHwnd())
        return;
    Layout();
    Invalidate(FALSE);
}

void CChatPanel::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Layout();
    Invalidate(FALSE);
}

void CChatPanel::Layout()
{
    if (!m_log.GetSafeHwnd())
        return;
    CRect cl;
    GetClientRect(&cl);
    int pad = S(14);
    m_closeBtn = CRect(cl.right - pad - S(26), S(10), cl.right - pad + S(2), S(38));

    int y = S(66);
    bool host = m_session && m_session->GetRole() == CCollabSession::ROLE_HOST && m_session->IsActive();
    if (host)
    {
        m_keyBox = CRect(pad, y + S(20), cl.right - pad, y + S(20) + S(36));
        m_copyBtn = CRect(m_keyBox.right - S(52), m_keyBox.top + S(5), m_keyBox.right - S(5), m_keyBox.bottom - S(5));
        y = m_keyBox.bottom + S(16);
    }
    else
    {
        m_keyBox.SetRectEmpty();
        m_copyBtn.SetRectEmpty();
    }

    m_usersTop = y;
    int count = m_session ? (int)m_session->Users().size() : 0;
    int rows = count == 0 ? 1 : min(count, MAX_VISIBLE_USERS) + (count > MAX_VISIBLE_USERS ? 1 : 0);
    y += S(24) + rows * S(24) + S(12);
    m_chatTop = y;

    m_inputBox = CRect(pad, cl.bottom - S(48), cl.right - pad, cl.bottom - S(12));
    m_sendBtn = CRect(m_inputBox.right - S(62), m_inputBox.top + S(4), m_inputBox.right - S(4), m_inputBox.bottom - S(4));
    CRect logRc(pad + S(1), m_chatTop + S(28), cl.right - pad - S(1), m_inputBox.top - S(12));
    if (logRc.bottom < logRc.top + S(40))
        logRc.bottom = logRc.top + S(40);
    m_log.MoveWindow(logRc);

    CClientDC dc(this);
    CFont* of = dc.SelectObject(&m_font);
    int th = dc.GetTextExtent(_T("가")).cy + S(4);
    dc.SelectObject(of);
    CRect in(m_inputBox.left + S(10), m_inputBox.top + (m_inputBox.Height() - th) / 2,
             m_sendBtn.left - S(8), m_inputBox.top + (m_inputBox.Height() - th) / 2 + th);
    m_input.MoveWindow(in);
}

COLORREF CChatPanel::ReadableColor(COLORREF c) const
{
    int lum = (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
    if (IsDarkMode() && lum < 120)
        return RGB((GetRValue(c) + 255) / 2, (GetGValue(c) + 255) / 2, (GetBValue(c) + 255) / 2);
    if (!IsDarkMode() && lum > 200)
        return RGB(GetRValue(c) / 2, GetGValue(c) / 2, GetBValue(c) / 2);
    return c;
}

// ---------------------------------------------------------------------------
// 채팅 기록

void CChatPanel::AddMessage(const CString& who, COLORREF color, const CString& text, bool system)
{
    ChatLine line;
    line.who = who;
    line.color = color;
    line.text = text;
    line.system = system;
    line.time = CTime::GetCurrentTime().Format(_T("%H:%M"));
    m_lines.push_back(line);
    if (m_lines.size() > MAX_LINES)
    {
        m_lines.erase(m_lines.begin(), m_lines.begin() + 100);
        RenderLog();
        return;
    }
    AppendLine(line);
}

void CChatPanel::AppendLine(const ChatLine& line)
{
    if (!m_log.GetSafeHwnd())
        return;
    const ThemeColors& th = Theme();
    auto add = [&](const CString& s, COLORREF c, bool bold, bool italic, int pt)
    {
        m_log.SetSel(-1, -1);
        CHARFORMAT2 cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_FACE | CFM_SIZE;
        cf.crTextColor = c;
        cf.dwEffects = (bold ? CFE_BOLD : 0) | (italic ? CFE_ITALIC : 0);
        cf.yHeight = pt * 20;
        _tcscpy_s(cf.szFaceName, _T("맑은 고딕"));
        m_log.SetSelectionCharFormat(cf);
        m_log.ReplaceSel(s);
    };
    CString nl = m_log.GetTextLength() > 0 ? _T("\n") : _T("");
    if (line.system)
    {
        add(nl + _T("· ") + line.text + _T("  ") + line.time, th.textDim, false, true, 9);
    }
    else
    {
        add(nl + line.who, ReadableColor(line.color), true, false, 9);
        add(_T("  ") + line.time + _T("\n"), th.textDim, false, false, 8);
        add(line.text, th.text, false, false, 10);
    }
    m_log.SendMessage(WM_VSCROLL, SB_BOTTOM, 0);
}

void CChatPanel::RenderLog()
{
    if (!m_log.GetSafeHwnd())
        return;
    m_log.SetRedraw(FALSE);
    m_log.SetWindowText(_T(""));
    for (auto& l : m_lines)
        AppendLine(l);
    m_log.SetRedraw(TRUE);
    m_log.Invalidate();
}

void CChatPanel::ApplyTheme()
{
    if (!m_log.GetSafeHwnd())
        return;
    m_log.SetBackgroundColor(FALSE, Theme().panelBg);
    ApplyThemeToControl(m_log.m_hWnd);
    RenderLog();
    m_input.Invalidate();
    Invalidate(FALSE);
}

void CChatPanel::DoSend()
{
    CString text;
    m_input.GetWindowText(text);
    text.Trim();
    if (text.IsEmpty())
        return;
    if (!m_session || !m_session->IsActive())
    {
        AddMessage(CString(), 0, _T("공유 중이 아니라서 메시지를 보낼 수 없습니다."), true);
        return;
    }
    if (OnSend)
        OnSend(text);
    m_input.SetWindowText(_T(""));
}

BOOL CChatPanel::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN && pMsg->hwnd == m_input.GetSafeHwnd())
    {
        DoSend();
        return TRUE;
    }
    return CWnd::PreTranslateMessage(pMsg);
}

// ---------------------------------------------------------------------------
// 그리기

BOOL CChatPanel::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

HBRUSH CChatPanel::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd && pWnd->GetSafeHwnd() == m_input.GetSafeHwnd())
    {
        COLORREF bg = Theme().hover;
        pDC->SetTextColor(Theme().text);
        pDC->SetBkColor(bg);
        if (m_inputBrushColor != bg)
        {
            m_inputBrush.DeleteObject();
            m_inputBrush.CreateSolidBrush(bg);
            m_inputBrushColor = bg;
        }
        return (HBRUSH)m_inputBrush.GetSafeHandle();
    }
    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CChatPanel::DrawButton(CDC& dc, const CRect& r, LPCTSTR text, bool hot, bool pressed, bool primary)
{
    const ThemeColors& th = Theme();
    COLORREF bg, fg;
    if (primary)
    {
        bg = th.accent;
        if (hot) bg = RGB(GetRValue(bg) * 85 / 100, GetGValue(bg) * 85 / 100, GetBValue(bg) * 85 / 100);
        fg = IsDarkMode() ? RGB(0, 0, 0) : RGB(255, 255, 255);
    }
    else
    {
        bg = pressed ? th.pressed : (hot ? th.separator : th.panelBg);
        fg = th.text;
    }
    CBrush br(bg);
    CPen pen(PS_SOLID, 1, primary ? bg : th.separator);
    CBrush* ob = dc.SelectObject(&br);
    CPen* op = dc.SelectObject(&pen);
    dc.RoundRect(r, CPoint(S(8), S(8)));
    dc.SelectObject(ob);
    dc.SelectObject(op);
    dc.SetTextColor(fg);
    CFont* of = dc.SelectObject(&m_bold);
    CRect tr = r;
    dc.DrawText(text, -1, tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    dc.SelectObject(of);
}

void CChatPanel::OnPaint()
{
    CPaintDC pdc(this);
    CRect cl;
    GetClientRect(&cl);
    CDC mem;
    mem.CreateCompatibleDC(&pdc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&pdc, cl.Width(), cl.Height());
    CBitmap* obmp = mem.SelectObject(&bmp);
    const ThemeColors& th = Theme();
    int pad = S(14);

    mem.FillSolidRect(cl, th.panelBg);
    mem.FillSolidRect(0, 0, 1, cl.Height(), th.panelLine);
    mem.SetBkMode(TRANSPARENT);

    // 제목 + 닫기
    CFont* of = mem.SelectObject(&m_title);
    mem.SetTextColor(th.text);
    mem.TextOut(pad, S(12), _T("공유 캔버스"));
    if (m_hot == HOT_CLOSE)
    {
        CBrush hb(th.hover);
        mem.FillRect(m_closeBtn, &hb);
    }
    {
        CPen xp(PS_SOLID, max(1, S(1)), th.text);
        CPen* op = mem.SelectObject(&xp);
        CPoint c = m_closeBtn.CenterPoint();
        int d = S(5);
        mem.MoveTo(c.x - d, c.y - d); mem.LineTo(c.x + d + 1, c.y + d + 1);
        mem.MoveTo(c.x + d, c.y - d); mem.LineTo(c.x - d - 1, c.y + d + 1);
        mem.SelectObject(op);
    }

    // 상태
    CString status;
    if (!m_session || m_session->GetRole() == CCollabSession::ROLE_NONE)
        status = _T("연결되어 있지 않습니다");
    else if (m_session->GetRole() == CCollabSession::ROLE_HOST)
        status = _T("호스트 · ") + m_session->HostAddress();
    else if (!m_session->IsActive())
        status = _T("연결 중... ") + m_session->HostAddress();
    else
        status = _T("참가 중 · 호스트 ") + m_session->HostAddress();
    mem.SelectObject(&m_font);
    mem.SetTextColor(th.textDim);
    mem.TextOut(pad, S(40), status);

    // 초대 키
    if (!m_keyBox.IsRectEmpty())
    {
        mem.SetTextColor(th.textDim);
        mem.TextOut(pad, m_keyBox.top - S(20), _T("초대 키 (참가자에게 알려 주세요)"));
        CBrush kb(th.hover);
        CPen kp(PS_SOLID, 1, th.separator);
        CBrush* ob = mem.SelectObject(&kb);
        CPen* op = mem.SelectObject(&kp);
        mem.RoundRect(m_keyBox, CPoint(S(8), S(8)));
        mem.SelectObject(ob);
        mem.SelectObject(op);
        mem.SelectObject(&m_mono);
        mem.SetTextColor(th.text);
        CRect kr(m_keyBox.left + S(8), m_keyBox.top, m_copyBtn.left - S(2), m_keyBox.bottom);
        mem.DrawText(m_session->InviteKey(), kr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        bool copied = m_copiedAt && ::GetTickCount() - m_copiedAt < 1500;
        DrawButton(mem, m_copyBtn, copied ? _T("복사됨") : _T("복사"), m_hot == HOT_COPY, m_pressed == HOT_COPY, false);
    }

    // 참가자
    int y = m_usersTop;
    const std::vector<CollabUser>* users = m_session ? &m_session->Users() : nullptr;
    int count = users ? (int)users->size() : 0;
    CString hdr;
    hdr.Format(_T("참가자 %d명"), count);
    mem.SelectObject(&m_bold);
    mem.SetTextColor(th.text);
    mem.TextOut(pad, y, hdr);
    y += S(24);
    mem.SelectObject(&m_font);
    if (count == 0)
    {
        mem.SetTextColor(th.textDim);
        mem.TextOut(pad, y + S(3), _T("공유 메뉴에서 시작하거나 참가하세요."));
    }
    else
    {
        for (int i = 0; i < count && i < MAX_VISIBLE_USERS; ++i)
        {
            const CollabUser& u = (*users)[i];
            int d = S(10);
            int cy = y + S(12);
            CBrush db(u.color);
            CPen dp(PS_SOLID, 1, u.color);
            CBrush* ob = mem.SelectObject(&db);
            CPen* op = mem.SelectObject(&dp);
            mem.Ellipse(pad + S(2), cy - d / 2, pad + S(2) + d, cy + d / 2 + (d & 1));
            mem.SelectObject(ob);
            mem.SelectObject(op);
            CString name = u.id;
            if (u.isHost) name += _T("  (호스트)");
            if (u.isMe) name += _T("  (나)");
            mem.SetTextColor(th.text);
            CRect tr(pad + S(20), y, cl.right - pad, y + S(24));
            mem.DrawText(name, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            y += S(24);
        }
        if (count > MAX_VISIBLE_USERS)
        {
            CString more;
            more.Format(_T("외 %d명"), count - MAX_VISIBLE_USERS);
            mem.SetTextColor(th.textDim);
            mem.TextOut(pad + S(20), y + S(4), more);
        }
    }

    // 채팅
    mem.FillSolidRect(pad, m_chatTop - S(6), cl.Width() - 2 * pad, 1, th.separator);
    mem.SelectObject(&m_bold);
    mem.SetTextColor(th.text);
    mem.TextOut(pad, m_chatTop + S(4), _T("채팅"));

    // 입력 상자
    {
        bool focused = ::GetFocus() == m_input.GetSafeHwnd();
        CBrush ib(th.hover);
        CPen ip(PS_SOLID, 1, focused ? th.accent : th.separator);
        CBrush* ob = mem.SelectObject(&ib);
        CPen* op = mem.SelectObject(&ip);
        mem.RoundRect(m_inputBox, CPoint(S(10), S(10)));
        mem.SelectObject(ob);
        mem.SelectObject(op);
        DrawButton(mem, m_sendBtn, _T("보내기"), m_hot == HOT_SEND, m_pressed == HOT_SEND, true);
    }

    mem.SelectObject(of);
    pdc.BitBlt(0, 0, cl.Width(), cl.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(obmp);
}

// ---------------------------------------------------------------------------
// 마우스

CChatPanel::HotButton CChatPanel::HitButton(CPoint pt) const
{
    if (m_copyBtn.PtInRect(pt)) return HOT_COPY;
    if (m_sendBtn.PtInRect(pt)) return HOT_SEND;
    if (m_closeBtn.PtInRect(pt)) return HOT_CLOSE;
    return HOT_NONE;
}

void CChatPanel::OnMouseMove(UINT, CPoint pt)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        ::TrackMouseEvent(&tme);
        m_tracking = true;
    }
    HotButton h = HitButton(pt);
    if (h != m_hot)
    {
        m_hot = h;
        Invalidate(FALSE);
    }
}

void CChatPanel::OnMouseLeave()
{
    m_tracking = false;
    m_hot = HOT_NONE;
    Invalidate(FALSE);
}

void CChatPanel::OnLButtonDown(UINT, CPoint pt)
{
    m_pressed = HitButton(pt);
    if (m_pressed != HOT_NONE)
        SetCapture();
    else if (m_inputBox.PtInRect(pt))
        m_input.SetFocus();
    Invalidate(FALSE);
}

void CChatPanel::OnLButtonUp(UINT, CPoint pt)
{
    HotButton p = m_pressed;
    m_pressed = HOT_NONE;
    if (GetCapture() == this)
        ReleaseCapture();
    Invalidate(FALSE);
    if (p == HOT_NONE || HitButton(pt) != p)
        return;
    switch (p)
    {
    case HOT_COPY:
        if (OnCopyKey) OnCopyKey();
        m_copiedAt = ::GetTickCount();
        SetTimer(TIMER_COPIED, 1600, nullptr);
        break;
    case HOT_SEND:
        DoSend();
        m_input.SetFocus();
        break;
    case HOT_CLOSE:
        if (OnHide) OnHide();
        break;
    default:
        break;
    }
}

void CChatPanel::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_COPIED)
    {
        KillTimer(TIMER_COPIED);
        m_copiedAt = 0;
        Invalidate(FALSE);
        return;
    }
    CWnd::OnTimer(nIDEvent);
}
