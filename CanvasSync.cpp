// CanvasSync.cpp : 공유 캔버스 - 변경 영역 동기화와 다른 참가자의 커서
#include "framework.h"
#include "CanvasWnd.h"
#include "MainFrm.h"
#include "Theme.h"

static const UINT_PTR TIMER_SYNC = 2;
static const UINT SYNC_INTERVAL_MS = 60;
static const DWORD CURSOR_INTERVAL_MS = 33;

void CCanvasWnd::StartSync()
{
    m_syncBase = m_img;
    m_syncActive = true;
    m_syncDirty = false;
    m_cursorShown = false;
    SetTimer(TIMER_SYNC, SYNC_INTERVAL_MS, nullptr);
    Invalidate(FALSE);
}

void CCanvasWnd::StopSync()
{
    m_syncActive = false;
    m_cursorPending = false;
    if (GetSafeHwnd())
        KillTimer(TIMER_SYNC);
    m_syncBase.Destroy();
    if (GetSafeHwnd())
        Invalidate(FALSE);
}

// 마지막으로 맞춘 그림과 비교해 바뀐 부분만 보낸다 (60ms마다 + 새 참가자가 들어올 때)
void CCanvasWnd::FlushSync()
{
    if (!m_syncActive)
        return;
    CCollabSession* s = Frame()->Collab();
    if (!s || !s->IsActive())
        return;

    if (m_cursorPending)
        SendCursorUpdate(m_hoverValid);

    if (!m_syncDirty)
        return;
    m_syncDirty = false;

    if (m_syncBase.Size() != m_img.Size())
    {
        s->SendFull(m_img);
        m_syncBase = m_img;
        return;
    }

    ::GdiFlush();
    int W = m_img.Width(), H = m_img.Height();
    int minX = W, minY = H, maxX = -1, maxY = -1;
    for (int y = 0; y < H; ++y)
    {
        const DWORD* a = m_img.Row(y);
        DWORD* b = m_syncBase.Row(y);
        if (memcmp(a, b, (size_t)W * 4) == 0)
            continue;
        bool rowChanged = false;
        for (int x = 0; x < W; ++x)
        {
            if ((a[x] ^ b[x]) & 0x00FFFFFF)
            {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                rowChanged = true;
            }
        }
        if (rowChanged)
        {
            if (y < minY) minY = y;
            maxY = y;
        }
        memcpy(b, a, (size_t)W * 4);    // 이 줄은 이제 맞춰진 상태
    }
    if (maxX < 0)
        return;
    s->SendEdit(m_img, CRect(minX, minY, maxX + 1, maxY + 1));
}

// 다른 참가자의 편집을 반영한다.
// 아직 보내지 않은 내 변경(그림 ≠ 기준 그림)은 남겨 두었다가 다음에 보낸다.
void CCanvasWnd::ApplyRemoteRegion(const CRect& rc, const std::vector<DWORD>& px)
{
    if (m_img.IsNull() || px.size() < (size_t)rc.Width() * rc.Height())
        return;
    CRect clip;
    if (!clip.IntersectRect(rc, CRect(0, 0, m_img.Width(), m_img.Height())))
        return;
    ::GdiFlush();
    bool haveBase = m_syncActive && m_syncBase.Size() == m_img.Size();
    for (int y = clip.top; y < clip.bottom; ++y)
    {
        const DWORD* src = &px[(size_t)(y - rc.top) * rc.Width() + (clip.left - rc.left)];
        DWORD* img = m_img.Row(y) + clip.left;
        DWORD* base = haveBase ? m_syncBase.Row(y) + clip.left : nullptr;
        for (int x = 0; x < clip.Width(); ++x)
        {
            if (!base || ((img[x] ^ base[x]) & 0x00FFFFFF) == 0)
                img[x] = src[x];
            if (base)
                base[x] = src[x];
        }
    }
    // 실행 취소 기록에도 반영해, 내 실행 취소가 다른 사람의 그림을 지우지 않게 한다
    auto patch = [&](CBitmap32& b)
    {
        if (b.Size() != m_img.Size())
            return;
        for (int y = clip.top; y < clip.bottom; ++y)
            memcpy(b.Row(y) + clip.left, &px[(size_t)(y - rc.top) * rc.Width() + (clip.left - rc.left)],
                   (size_t)clip.Width() * 4);
    };
    for (auto& b : m_undo) patch(b);
    for (auto& b : m_redo) patch(b);

    CRect s = ImgRectToScreen(clip);
    s.InflateRect(2, 2);
    InvalidateRect(s, FALSE);
}

void CCanvasWnd::ApplyRemoteFull(int w, int h, const std::vector<DWORD>& px)
{
    CBitmap32 b;
    if (px.size() < (size_t)w * h || !b.Create(w, h, 0xFFFFFFFF))
        return;
    ::GdiFlush();
    memcpy(b.Row(0), px.data(), (size_t)w * h * 4);
    m_img = b;
    if (m_syncActive)
        m_syncBase = std::move(b);
    m_undo.clear();
    m_redo.clear();
    UpdateTextEditFont();
    OnImageSizeChanged();
}

void CCanvasWnd::SendCursorUpdate(bool visible)
{
    if (!m_syncActive)
        return;
    CCollabSession* s = Frame()->Collab();
    if (!s || !s->IsActive())
        return;
    DWORD now = ::GetTickCount();
    if (visible && now - m_lastCursorSend < CURSOR_INTERVAL_MS)
    {
        m_cursorPending = true;     // 다음 타이머에서 보낸다
        return;
    }
    if (!visible && !m_cursorShown)
        return;
    s->SendCursor(m_hoverImg, visible);
    m_lastCursorSend = now;
    m_cursorPending = false;
    m_cursorShown = visible;
}

void CCanvasWnd::InvalidateRemoteCursor(CPoint imgPos)
{
    if (!GetSafeHwnd())
        return;
    CPoint p = ImgToScreen(imgPos.x, imgPos.y);
    int k = m_dpi;
    CRect r(p.x - 4, p.y - 4, p.x + Z(1) + MulDiv(260, k, 96), p.y + Z(1) + MulDiv(52, k, 96));
    InvalidateRect(r, FALSE);
}

void CCanvasWnd::DrawRemoteCursors(CDC& dc)
{
    CCollabSession* s = Frame()->Collab();
    if (!m_syncActive || !s || !s->IsActive())
        return;
    if (!m_labelFont.GetSafeHandle())
    {
        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(9, m_dpi, 72);
        lf.lfWeight = FW_SEMIBOLD;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("맑은 고딕"));
        m_labelFont.CreateFontIndirect(&lf);
    }
    double k = m_dpi / 96.0;
    static const POINT arrow[7] = { {0,0}, {0,17}, {4,13}, {7,19}, {10,18}, {7,12}, {12,12} };
    CFont* of = dc.SelectObject(&m_labelFont);
    for (const CollabUser& u : s->Users())
    {
        if (u.isMe || !u.cursorVisible)
            continue;
        int half = Z(1) / 2;
        CPoint p = ImgToScreen(u.cursor.x, u.cursor.y) + CPoint(half, half);

        POINT pts[7];
        for (int i = 0; i < 7; ++i)
            pts[i] = { p.x + (LONG)lround(arrow[i].x * k), p.y + (LONG)lround(arrow[i].y * k) };
        CBrush br(u.color);
        CPen pen(PS_SOLID, 1, RGB(255, 255, 255));
        CBrush* ob = dc.SelectObject(&br);
        CPen* op = dc.SelectObject(&pen);
        dc.Polygon(pts, 7);

        // 아이디 꼬리표
        CSize ts = dc.GetTextExtent(u.id);
        int padX = (int)(6 * k), padY = (int)(2 * k);
        CRect lr(p.x + (int)(10 * k), p.y + (int)(20 * k), 0, 0);
        lr.right = lr.left + ts.cx + 2 * padX;
        lr.bottom = lr.top + ts.cy + 2 * padY;
        CPen none(PS_NULL, 0, RGB(0, 0, 0));
        dc.SelectObject(&none);
        dc.RoundRect(lr, CPoint((int)(8 * k), (int)(8 * k)));
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(RGB(255, 255, 255));
        dc.DrawText(u.id, lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        dc.SelectObject(ob);
        dc.SelectObject(op);
    }
    dc.SelectObject(of);
}
