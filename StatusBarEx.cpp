// StatusBarEx.cpp : 테마 상태 표시줄과 확대/축소 슬라이더
#include "StatusBarEx.h"
#include "Theme.h"

const double CPaintStatusBar::MIN_ZOOM = 0.125;
const double CPaintStatusBar::MAX_ZOOM = 8.0;

BEGIN_MESSAGE_MAP(CPaintStatusBar, CStatusBar)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_NCPAINT()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

// 12.5% ~ 800% 를 로그 비율로 슬라이더에 배치
double CPaintStatusBar::ZoomToPos(double z)
{
    z = ClampV(z, MIN_ZOOM, MAX_ZOOM);
    return log(z / MIN_ZOOM) / log(MAX_ZOOM / MIN_ZOOM);
}

double CPaintStatusBar::PosToZoom(double t)
{
    t = ClampV(t, 0.0, 1.0);
    double z = MIN_ZOOM * pow(MAX_ZOOM / MIN_ZOOM, t);
    if (fabs(t - ZoomToPos(1.0)) < 0.025)
        z = 1.0;    // 100% 근처에서는 100%로 맞춘다
    return z;
}

void CPaintStatusBar::SetZoom(double z)
{
    if (fabs(z - m_zoom) < 1e-9)
        return;
    m_zoom = z;
    if (GetSafeHwnd())
    {
        CRect r;
        GetItemRect(ZOOM_PANE, &r);
        InvalidateRect(r, FALSE);
    }
}

void CPaintStatusBar::GetSliderParts(CRect& minus, CRect& track, CRect& plus) const
{
    CRect r;
    GetItemRect(ZOOM_PANE, &r);
    int bw = r.Height();
    minus = CRect(r.left, r.top, r.left + bw, r.bottom);
    plus = CRect(r.right - bw, r.top, r.right, r.bottom);
    track = CRect(minus.right + 4, r.top, plus.left - 4, r.bottom);
}

void CPaintStatusBar::SetZoomFromX(int x)
{
    CRect minus, track, plus;
    GetSliderParts(minus, track, plus);
    double t = (double)(x - track.left) / max(1, (int)track.Width());
    double z = PosToZoom(t);
    if (OnZoomChanged)
        OnZoomChanged(z);
}

BOOL CPaintStatusBar::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CPaintStatusBar::OnNcPaint()
{
    // 테두리 영역도 테마 색으로 칠한다
    CWindowDC dc(this);
    CRect rw, rc;
    GetWindowRect(&rw);
    GetClientRect(&rc);
    ClientToScreen(&rc);
    rc.OffsetRect(-rw.left, -rw.top);
    rw.OffsetRect(-rw.left, -rw.top);
    dc.ExcludeClipRect(rc);
    dc.FillSolidRect(rw, Theme().statusBg);
}

static void DrawPaneIcon(CDC& dc, int kind, CRect r, COLORREF c)
{
    // kind 1: 커서 위치, 2: 선택 크기, 3: 이미지 크기
    int s = r.Height() / 2;
    int x = r.left + 4, y = r.top + (r.Height() - s) / 2;
    CPen pen(PS_SOLID, 1, c);
    CPen* op = dc.SelectObject(&pen);
    CGdiObject* ob = dc.SelectStockObject(NULL_BRUSH);
    switch (kind)
    {
    case 1:
        dc.MoveTo(x + s / 2, y); dc.LineTo(x + s / 2, y + s + 1);
        dc.MoveTo(x, y + s / 2); dc.LineTo(x + s + 1, y + s / 2);
        break;
    case 2:
    {
        CPen dot(PS_DOT, 1, c);
        dc.SelectObject(&dot);
        dc.Rectangle(x, y, x + s + 1, y + s + 1);
        dc.SelectObject(&pen);
        break;
    }
    case 3:
        dc.Rectangle(x, y + 1, x + s + 2, y + s);
        dc.MoveTo(x + 2, y + s - 2); dc.LineTo(x + s / 2, y + s / 2); dc.LineTo(x + s, y + s - 2);
        break;
    }
    dc.SelectObject(ob);
    dc.SelectObject(op);
}

void CPaintStatusBar::OnPaint()
{
    CPaintDC pdc(this);
    CRect cl;
    GetClientRect(&cl);
    CDC mem;
    mem.CreateCompatibleDC(&pdc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&pdc, cl.Width(), cl.Height());
    CBitmap* ob = mem.SelectObject(&bmp);

    const ThemeColors& th = Theme();
    mem.FillSolidRect(cl, th.statusBg);
    mem.FillSolidRect(0, 0, cl.Width(), 1, th.statusLine);

    CFont* of = mem.SelectObject(GetFont());
    mem.SetBkMode(TRANSPARENT);
    mem.SetTextColor(th.statusText);

    int count = GetCount();
    for (int i = 0; i < count; ++i)
    {
        CRect r;
        GetItemRect(i, &r);
        if (i > 0 && i < ZOOM_PANE)
            mem.FillSolidRect(r.left - 1, r.top + 4, 1, r.Height() - 8, th.statusLine);
        if (i == ZOOM_PANE)
        {
            CRect minus, track, plus;
            GetSliderParts(minus, track, plus);
            COLORREF ink = th.statusText;
            int cy = (r.top + r.bottom) / 2;
            int half = minus.Height() / 5;
            // − / +
            mem.FillSolidRect(minus.CenterPoint().x - half, cy, half * 2 + 1, 1, ink);
            mem.FillSolidRect(plus.CenterPoint().x - half, cy, half * 2 + 1, 1, ink);
            mem.FillSolidRect(plus.CenterPoint().x, cy - half, 1, half * 2 + 1, ink);
            // 트랙
            int tx = track.left + (int)lround(ZoomToPos(m_zoom) * track.Width());
            mem.FillSolidRect(track.left, cy - 1, track.Width(), 2, th.textDim);
            mem.FillSolidRect(track.left, cy - 1, tx - track.left, 2, th.accent);
            // 100% 위치 표시
            int t100 = track.left + (int)lround(ZoomToPos(1.0) * track.Width());
            mem.FillSolidRect(t100, cy - 4, 1, 8, th.textDim);
            // 손잡이
            int tr = max(4, r.Height() / 4);
            CBrush thumb(th.accent);
            CPen thumbPen(PS_SOLID, 1, th.accent);
            CBrush* obr = mem.SelectObject(&thumb);
            CPen* opn = mem.SelectObject(&thumbPen);
            mem.Ellipse(tx - tr, cy - tr, tx + tr + 1, cy + tr + 1);
            mem.SelectObject(obr);
            mem.SelectObject(opn);
            continue;
        }
        CString text = GetPaneText(i);
        CRect tr = r;
        if (i >= 1 && i <= 3)
        {
            if (!text.IsEmpty())
                DrawPaneIcon(mem, i, r, th.statusText);
            tr.left += r.Height() / 2 + 10;
        }
        else
            tr.left += 6;
        mem.DrawText(text, tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    // 크기 조정 손잡이 (점 3개)
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j <= i; ++j)
            mem.FillSolidRect(cl.right - 4 - (i - j) * 4, cl.bottom - 4 - j * 4, 2, 2, th.textDim);

    mem.SelectObject(of);
    pdc.BitBlt(0, 0, cl.Width(), cl.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(ob);
}

void CPaintStatusBar::OnLButtonDown(UINT nFlags, CPoint pt)
{
    CRect minus, track, plus;
    GetSliderParts(minus, track, plus);
    if (minus.PtInRect(pt))
    {
        if (OnZoomStep) OnZoomStep(-1);
        return;
    }
    if (plus.PtInRect(pt))
    {
        if (OnZoomStep) OnZoomStep(1);
        return;
    }
    CRect hit = track;
    hit.InflateRect(4, 0);
    if (hit.PtInRect(pt))
    {
        m_dragging = true;
        SetCapture();
        SetZoomFromX(pt.x);
        return;
    }
    CStatusBar::OnLButtonDown(nFlags, pt);
}

void CPaintStatusBar::OnMouseMove(UINT nFlags, CPoint pt)
{
    if (m_dragging)
    {
        SetZoomFromX(pt.x);
        return;
    }
    CStatusBar::OnMouseMove(nFlags, pt);
}

void CPaintStatusBar::OnLButtonUp(UINT nFlags, CPoint pt)
{
    if (m_dragging)
    {
        m_dragging = false;
        if (GetCapture() == this)
            ReleaseCapture();
        return;
    }
    CStatusBar::OnLButtonUp(nFlags, pt);
}
