// CanvasInput.cpp : 마우스/키보드 입력과 그리기 도구, 선택 영역, 텍스트 편집
#include "framework.h"
#include "resource.h"
#include "CanvasWnd.h"
#include "MainFrm.h"

static const UINT_PTR TIMER_AIRBRUSH = 1;

template <class F>
static void ForLine(CPoint a, CPoint b, int step, F f)
{
    int dx = b.x - a.x, dy = b.y - a.y;
    int n = max(abs(dx), abs(dy));
    if (n == 0)
    {
        f(a.x, a.y);
        return;
    }
    int i = 0;
    for (; i <= n; i += step)
        f(a.x + (int)lround((double)dx * i / n), a.y + (int)lround((double)dy * i / n));
    if (i - step != n)
        f(b.x, b.y);
}

// ---------------------------------------------------------------------------
// 마우스 메시지

void CCanvasWnd::OnLButtonDown(UINT nFlags, CPoint pt) { MouseDown(pt, 0, nFlags); }
void CCanvasWnd::OnRButtonDown(UINT nFlags, CPoint pt) { MouseDown(pt, 1, nFlags); }
void CCanvasWnd::OnLButtonUp(UINT nFlags, CPoint pt)
{
    if (m_drag != DRAG_NONE && m_dragButton == 0)
    {
        MouseMove(pt, nFlags);   // 마지막 위치를 반영한 뒤 끝낸다
        MouseUp(pt);
    }
}

// 다각형은 두 번 클릭으로 마무리한다. 그 밖에는 일반 클릭으로 처리.
void CCanvasWnd::OnLButtonDblClk(UINT nFlags, CPoint pt)
{
    if (m_polyActive && m_drag == DRAG_NONE)
    {
        CPoint ip = ScreenToImg(pt);
        if (m_polyPts.empty() || m_polyPts.back() != ip)
            m_polyPts.push_back(ip);
        CommitPolygon();
        return;
    }
    OnLButtonDown(nFlags, pt);
}

void CCanvasWnd::OnRButtonUp(UINT nFlags, CPoint pt)
{
    if (m_drag != DRAG_NONE && m_dragButton == 1)
    {
        MouseMove(pt, nFlags);
        MouseUp(pt);
    }
}

void CCanvasWnd::OnMouseMove(UINT nFlags, CPoint pt)
{
    if (!m_trackingLeave)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        ::TrackMouseEvent(&tme);
        m_trackingLeave = true;
    }
    MouseMove(pt, nFlags);
}

void CCanvasWnd::OnMouseLeave()
{
    m_trackingLeave = false;
    m_hoverIn = false;
    m_hoverValid = false;
    m_cursorPending = false;
    SendCursorUpdate(false);
    UpdateStatus();
    InvalidateRulers();
    if (m_tool == TOOL_ERASER)
        Invalidate(FALSE);
}

void CCanvasWnd::OnCaptureChanged(CWnd* pWnd)
{
    if (pWnd != this && m_drag != DRAG_NONE)
        MouseUp(m_lastMouse);
    CWnd::OnCaptureChanged(pWnd);
}

void CCanvasWnd::MouseDown(CPoint pt, int button, UINT flags)
{
    if (m_drag != DRAG_NONE)
        return;
    SetFocus();
    if (!ViewRect().PtInRect(pt))
        return;     // 눈금자 영역
    m_lastMouse = pt;
    CPoint ip = ScreenToImg(pt);
    m_dragButton = button;

    // 1. 캔버스 크기 조정 핸들
    if (button == 0 && !m_selActive && !IsShapeBusy() && !m_textActive)
    {
        int h = HitCanvasHandle(pt);
        if (h >= 0)
        {
            m_drag = DRAG_CANVAS;
            m_dragHandle = h;
            m_canvasNewSize = m_img.Size();
            SetCapture();
            Invalidate(FALSE);
            return;
        }
    }

    // 2. 확정 전 도형: 핸들/이동, 바깥 클릭 시 확정
    if (m_shapePending)
    {
        if (button == 0)
        {
            int h = HitShapeHandle(pt);
            if (h >= 0)
            {
                m_drag = DRAG_SHAPE_HANDLE;
                m_dragHandle = h;
                m_dragOrigP1 = m_sp1;
                m_dragOrigP2 = m_sp2;
                m_dragOrigRect = CRect(min(m_sp1.x, m_sp2.x), min(m_sp1.y, m_sp2.y),
                                       max(m_sp1.x, m_sp2.x) + 1, max(m_sp1.y, m_sp2.y) + 1);
                SetCapture();
                return;
            }
            CRect s = ShapeScreenRect();
            s.InflateRect(4, 4);
            if (s.PtInRect(pt))
            {
                m_drag = DRAG_SHAPE_MOVE;
                m_dragStartImg = ip;
                m_dragOrigP1 = m_sp1;
                m_dragOrigP2 = m_sp2;
                SetCapture();
                return;
            }
        }
        CommitShape();
    }

    // 3. 선택 영역: 핸들/이동, 바깥 클릭 시 확정
    if (m_selActive)
    {
        if (button == 0)
        {
            CRect s = ImgRectToScreen(m_selRect);
            int h = HitHandle(pt, s);
            if (h >= 0)
            {
                Lift();
                m_drag = DRAG_SEL_HANDLE;
                m_dragHandle = h;
                m_dragOrigRect = m_selRect;
                SetCapture();
                return;
            }
            if (s.PtInRect(pt))
            {
                if (m_lifted && (flags & MK_CONTROL))
                    CompositeFloat(m_img, m_selRect.left, m_selRect.top);   // Ctrl+끌기: 복제
                else
                    Lift();
                m_drag = DRAG_SEL_MOVE;
                m_dragStartImg = ip;
                m_dragOrigRect = m_selRect;
                SetCapture();
                return;
            }
        }
        CommitSelection();
    }

    // 4. 텍스트 편집 중이면 확정
    if (m_textActive)
    {
        CommitText();
        if (m_tool == TOOL_TEXT)
            return;
    }

    bool inImg = m_img.InBounds(ip.x, ip.y);
    CPoint cp(ClampV((int)ip.x, 0, m_img.Width() - 1), ClampV((int)ip.y, 0, m_img.Height() - 1));

    switch (m_tool)
    {
    case TOOL_SELECT_RECT:
        m_drag = DRAG_SEL_NEW;
        m_dragStartImg = cp;
        m_selRect = CRect(cp, CSize(0, 0));
        break;
    case TOOL_SELECT_FREE:
        m_drag = DRAG_SEL_FREE;
        m_freePoly.clear();
        m_freePoly.push_back(cp);
        break;
    case TOOL_PENCIL:
    case TOOL_ERASER:
    case TOOL_BRUSH:
        PushUndo();
        m_drag = DRAG_STROKE;
        m_lastImg = ip;
        BeginStroke();
        StrokeSegment(ip, ip);
        break;
    case TOOL_FILL:
        if (inImg)
            FloodFill(ip, m_color[button]);
        break;
    case TOOL_PICKER:
        if (inImg)
        {
            ::GdiFlush();
            SetColor(button, ToColor(m_img.GetPixel(ip.x, ip.y)));
            SetTool(m_prevTool == TOOL_PICKER ? TOOL_PENCIL : m_prevTool);
        }
        return;
    case TOOL_MAGNIFIER:
        ZoomStep(button == 0 ? 1 : -1, pt);
        return;
    case TOOL_SHAPE:
        if (m_shape == SH_CURVE)
        {
            // 1단계: 선 긋기 → 2단계: 첫 번째 휘기 → 3단계: 두 번째 휘기
            if (m_curveStage == 0)
            {
                m_cv[0] = m_cv[1] = m_cv[2] = m_cv[3] = ip;
                m_curveSwap = (button == 1);
            }
            else if (m_curveStage == 1)
                m_cv[1] = m_cv[2] = ip;
            else
                m_cv[2] = ip;
            m_drag = DRAG_CURVE;
        }
        else if (m_shape == SH_POLYGON)
        {
            if (!m_polyActive)
            {
                m_polyActive = true;
                m_polyPts.assign(1, ip);
                m_polySwap = (button == 1);
            }
            m_polyCursor = ip;
            m_drag = DRAG_POLY;
        }
        else
        {
            m_drag = DRAG_SHAPE_NEW;
            m_sp1 = m_sp2 = ip;
            m_shapeSwap = (button == 1);
            m_pendingShape = m_shape;
        }
        break;
    case TOOL_TEXT:
        if (button == 0)
        {
            m_drag = DRAG_TEXT_NEW;
            m_dragStartImg = ip;
            m_textRect = CRect(ip, CSize(0, 0));
        }
        break;
    }

    if (m_drag != DRAG_NONE)
        SetCapture();
    Invalidate(FALSE);
    UpdateStatus();
}

void CCanvasWnd::MouseMove(CPoint pt, UINT flags)
{
    m_lastMouse = pt;
    CPoint ip = ScreenToImg(pt);
    bool oldIn = m_hoverIn;
    CPoint oldHover = m_hoverImg;
    m_hoverImg = ip;
    m_hoverIn = m_img.InBounds(ip.x, ip.y);
    m_hoverValid = ViewRect().PtInRect(pt) != FALSE;
    bool shift = (flags & MK_SHIFT) != 0;
    CPoint cp(ClampV((int)ip.x, 0, m_img.Width() - 1), ClampV((int)ip.y, 0, m_img.Height() - 1));
    if (oldHover != m_hoverImg)
    {
        InvalidateRulers();
        if (m_hoverValid)
            SendCursorUpdate(true);
    }

    switch (m_drag)
    {
    case DRAG_NONE:
        UpdateStatus();
        if (m_polyActive)
        {
            // 다음 꼭짓점까지 이어질 선을 미리 보여준다
            m_polyCursor = shift ? ConstrainLine(m_polyPts.back(), ip) : ip;
            Invalidate(FALSE);
        }
        else if (m_tool == TOOL_ERASER && (oldIn != m_hoverIn || oldHover != m_hoverImg))
            Invalidate(FALSE);
        return;
    case DRAG_CURVE:
        if (m_curveStage == 0)
        {
            m_cv[3] = shift ? ConstrainLine(m_cv[0], ip) : ip;
            m_cv[1] = m_cv[0];
            m_cv[2] = m_cv[3];
        }
        else if (m_curveStage == 1)
            m_cv[1] = m_cv[2] = ip;
        else
            m_cv[2] = ip;
        break;
    case DRAG_POLY:
        m_polyCursor = shift ? ConstrainLine(m_polyPts.back(), ip) : ip;
        break;
    case DRAG_STROKE:
        if (ip != m_lastImg)
        {
            StrokeSegment(m_lastImg, ip);
            m_lastImg = ip;
        }
        break;
    case DRAG_SHAPE_NEW:
        m_sp2 = shift ? ConstrainShape(m_sp1, ip) : ip;
        break;
    case DRAG_SHAPE_MOVE:
    {
        CPoint d = ip - m_dragStartImg;
        m_sp1 = m_dragOrigP1 + d;
        m_sp2 = m_dragOrigP2 + d;
        break;
    }
    case DRAG_SHAPE_HANDLE:
        if (m_pendingShape == SH_LINE)
        {
            if (m_dragHandle == 0)
                m_sp1 = shift ? ConstrainShape(m_sp2, ip) : ip;
            else
                m_sp2 = shift ? ConstrainShape(m_sp1, ip) : ip;
        }
        else
        {
            CRect r = ResizeRectByHandle(m_dragOrigRect, m_dragHandle, ip);
            m_sp1 = r.TopLeft();
            m_sp2 = CPoint(r.right - 1, r.bottom - 1);
        }
        break;
    case DRAG_SEL_NEW:
    {
        CRect r(min(m_dragStartImg.x, cp.x), min(m_dragStartImg.y, cp.y),
                max(m_dragStartImg.x, cp.x) + 1, max(m_dragStartImg.y, cp.y) + 1);
        m_selRect = r;
        break;
    }
    case DRAG_SEL_FREE:
        if (m_freePoly.empty() || m_freePoly.back() != cp)
            m_freePoly.push_back(cp);
        break;
    case DRAG_SEL_MOVE:
    {
        CPoint d = ip - m_dragStartImg;
        m_selRect = m_dragOrigRect;
        m_selRect.OffsetRect(d);
        break;
    }
    case DRAG_SEL_HANDLE:
        m_selRect = ResizeRectByHandle(m_dragOrigRect, m_dragHandle, ip);
        break;
    case DRAG_CANVAS:
    {
        int w = m_canvasNewSize.cx, h = m_canvasNewSize.cy;
        if (m_dragHandle == 0 || m_dragHandle == 1) w = ClampV((int)ip.x, 1, 20000);
        if (m_dragHandle == 1 || m_dragHandle == 2) h = ClampV((int)ip.y, 1, 20000);
        m_canvasNewSize = CSize(w, h);
        break;
    }
    case DRAG_TEXT_NEW:
        m_textRect = CRect(min(m_dragStartImg.x, ip.x), min(m_dragStartImg.y, ip.y),
                           max(m_dragStartImg.x, ip.x) + 1, max(m_dragStartImg.y, ip.y) + 1);
        break;
    }
    Invalidate(FALSE);
    UpdateStatus();
}

void CCanvasWnd::MouseUp(CPoint pt)
{
    DragMode mode = m_drag;
    m_drag = DRAG_NONE;
    m_syncDirty = true;
    if (GetCapture() == this)
        ReleaseCapture();

    switch (mode)
    {
    case DRAG_STROKE:
        EndStroke();
        break;
    case DRAG_SHAPE_NEW:
        m_shapePending = (m_sp1 != m_sp2);
        break;
    case DRAG_CURVE:
        if (m_curveStage == 0)
        {
            if (CPoint(m_cv[0]) != CPoint(m_cv[3]))
                m_curveStage = 1;
        }
        else if (m_curveStage == 1)
            m_curveStage = 2;
        else
            CommitCurve();
        break;
    case DRAG_POLY:
    {
        CPoint p = m_polyCursor;
        CPoint first = m_polyPts.front();
        // 시작점 근처(화면에서 6px 이내)를 클릭하면 닫는다
        double dist = hypot((double)(p.x - first.x), (double)(p.y - first.y)) * m_zoom;
        if (m_polyPts.size() >= 3 && dist <= 6.0)
            CommitPolygon();
        else if (p != m_polyPts.back())
            m_polyPts.push_back(p);
        break;
    }
    case DRAG_SEL_NEW:
        m_selActive = m_selRect.Width() > 1 || m_selRect.Height() > 1;
        m_selIsFree = false;
        m_lifted = false;
        break;
    case DRAG_SEL_FREE:
    {
        m_selActive = false;
        if (m_freePoly.size() >= 3)
        {
            CRect r(INT_MAX, INT_MAX, INT_MIN, INT_MIN);
            for (auto& p : m_freePoly)
            {
                r.left = min(r.left, p.x);  r.top = min(r.top, p.y);
                r.right = max(r.right, p.x + 1); r.bottom = max(r.bottom, p.y + 1);
            }
            if (r.Width() > 1 && r.Height() > 1)
            {
                m_selRect = r;
                m_selActive = true;
                m_selIsFree = true;
                m_lifted = false;
            }
        }
        if (!m_selActive)
            m_freePoly.clear();
        break;
    }
    case DRAG_SEL_HANDLE:
        m_floatDirty = true;
        break;
    case DRAG_CANVAS:
        if (m_canvasNewSize != m_img.Size())
        {
            PushUndo();
            ResizeCanvas(m_canvasNewSize.cx, m_canvasNewSize.cy);
            OnImageSizeChanged();
        }
        break;
    case DRAG_TEXT_NEW:
    {
        CRect r = m_textRect;
        int lineH = (int)(PtToPx(m_fontPt) * 1.5) + 6;
        if (r.Width() < 12 || r.Height() < 8)
            r = CRect(m_dragStartImg, CSize(max(220, lineH * 6), lineH));
        if (r.Height() < lineH)
            r.bottom = r.top + lineH;
        BeginTextEdit(r);
        break;
    }
    default:
        break;
    }
    (void)pt;
    Invalidate(FALSE);
    UpdateStatus();
    NotifyState();
}

BOOL CCanvasWnd::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    if (nHitTest != HTCLIENT || pWnd != this)
        return CWnd::OnSetCursor(pWnd, nHitTest, message);

    static const LPCTSTR handleCursors[8] =
    { IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE, IDC_SIZENWSE, IDC_SIZENS, IDC_SIZENESW, IDC_SIZEWE };

    CPoint pt;
    ::GetCursorPos(&pt);
    ScreenToClient(&pt);
    LPCTSTR cur = IDC_ARROW;
    CPoint ip = ScreenToImg(pt);

    if (m_drag == DRAG_NONE && !ViewRect().PtInRect(pt))
        cur = IDC_ARROW;    // 눈금자
    else if (m_drag == DRAG_SEL_MOVE || m_drag == DRAG_SHAPE_MOVE)
        cur = IDC_SIZEALL;
    else if (m_drag == DRAG_CANVAS)
        cur = m_dragHandle == 0 ? IDC_SIZEWE : (m_dragHandle == 1 ? IDC_SIZENWSE : IDC_SIZENS);
    else if (m_drag == DRAG_SEL_HANDLE || (m_drag == DRAG_SHAPE_HANDLE && m_pendingShape != SH_LINE))
        cur = handleCursors[m_dragHandle];
    else if (m_drag == DRAG_NONE && !m_selActive && !IsShapeBusy() && !m_textActive && HitCanvasHandle(pt) >= 0)
    {
        int h = HitCanvasHandle(pt);
        cur = h == 0 ? IDC_SIZEWE : (h == 1 ? IDC_SIZENWSE : IDC_SIZENS);
    }
    else if (m_drag == DRAG_NONE && m_shapePending && HitShapeHandle(pt) >= 0)
        cur = m_pendingShape == SH_LINE ? IDC_SIZEALL : handleCursors[HitShapeHandle(pt)];
    else if (m_drag == DRAG_NONE && m_shapePending && ShapeScreenRect().PtInRect(pt))
        cur = IDC_SIZEALL;
    else if (m_drag == DRAG_NONE && m_selActive && HitHandle(pt, ImgRectToScreen(m_selRect)) >= 0)
        cur = handleCursors[HitHandle(pt, ImgRectToScreen(m_selRect))];
    else if (m_drag == DRAG_NONE && m_selActive && ImgRectToScreen(m_selRect).PtInRect(pt))
        cur = IDC_SIZEALL;
    else if (m_img.InBounds(ip.x, ip.y) || m_drag != DRAG_NONE)
    {
        switch (m_tool)
        {
        case TOOL_TEXT: cur = IDC_IBEAM; break;
        case TOOL_MAGNIFIER: cur = IDC_HAND; break;
        default: cur = IDC_CROSS; break;
        }
    }
    ::SetCursor(::LoadCursor(nullptr, cur));
    return TRUE;
}

void CCanvasWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    bool shift = ::GetKeyState(VK_SHIFT) < 0;
    int step = shift ? 10 : 1;
    switch (nChar)
    {
    case VK_ESCAPE:
    case VK_RETURN:
        if (m_drag != DRAG_NONE)
            MouseUp(m_lastMouse);
        CommitShape();
        CommitSelection();
        Invalidate(FALSE);
        NotifyState();
        return;
    case VK_BACK:
        // 다각형: 마지막 꼭짓점 지우기
        if (m_polyActive && m_drag == DRAG_NONE)
        {
            m_polyPts.pop_back();
            if (m_polyPts.empty())
                m_polyActive = false;
            Invalidate(FALSE);
            NotifyState();
        }
        return;
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
    {
        CPoint d(nChar == VK_LEFT ? -step : (nChar == VK_RIGHT ? step : 0),
                 nChar == VK_UP ? -step : (nChar == VK_DOWN ? step : 0));
        if (m_selActive)
        {
            Lift();
            m_selRect.OffsetRect(d);
        }
        else if (m_shapePending)
        {
            m_sp1 += d;
            m_sp2 += d;
        }
        else
        {
            ScrollTo(m_scroll.x + d.x * 20, m_scroll.y + d.y * 20);
            return;
        }
        Invalidate(FALSE);
        UpdateStatus();
        return;
    }
    }
    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CCanvasWnd::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_AIRBRUSH && m_drag == DRAG_STROKE)
    {
        Spray(m_lastImg);
        m_syncDirty = true;
        Invalidate(FALSE);
        return;
    }
    if (nIDEvent == 2)     // 공유 캔버스 동기화 (CanvasSync.cpp)
    {
        FlushSync();
        return;
    }
    CWnd::OnTimer(nIDEvent);
}

// ---------------------------------------------------------------------------
// 붓 획

// 위치마다 고정된 0~1 값 (종이 결 표현용)
static float Grain(int x, int y)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFF) / 65535.0f;
}

static double Rand01()
{
    return rand() / (double)RAND_MAX;
}

void CCanvasWnd::BeginStroke()
{
    if (m_tool != TOOL_BRUSH)
        return;
    switch (m_brush)
    {
    case BRUSH_MARKER:
    case BRUSH_WATERCOLOR:
    case BRUSH_NATURAL_PENCIL:
        m_strokeAlpha.assign((size_t)m_img.Width() * m_img.Height(), 0);
        break;
    case BRUSH_AIR:
        SetTimer(TIMER_AIRBRUSH, 30, nullptr);
        break;
    case BRUSH_OIL:
    {
        // 붓털마다 위치·밝기·굵기를 조금씩 다르게
        int w = max(3, GetToolWidth());
        int count = max(3, w / 2);
        double spacing = (double)w / count;
        m_bristles.clear();
        for (int i = 0; i < count; ++i)
        {
            Bristle b;
            b.offset = -w / 2.0 + spacing * (i + 0.5) + (Rand01() - 0.5) * spacing * 0.6;
            b.shade = 0.72 + Rand01() * 0.50;
            // 이웃 붓털과 겹치도록 간격보다 굵게 (가끔 가는 붓털로 결을 남긴다)
            b.width = (int)ceil(spacing) + 1 + (Rand01() < 0.25 ? -1 : (Rand01() < 0.5 ? 1 : 0));
            if (b.width < 1) b.width = 1;
            m_bristles.push_back(b);
        }
        m_oilDirX = 1.0;
        m_oilDirY = 0.0;
        break;
    }
    default:
        break;
    }
}

void CCanvasWnd::EndStroke()
{
    KillTimer(TIMER_AIRBRUSH);
    m_strokeAlpha.clear();
    m_strokeAlpha.shrink_to_fit();
    m_bristles.clear();
}

void CCanvasWnd::DrawPenLine(CPoint a, CPoint b, int w, COLORREF c)
{
    CBitmapDC dc(m_img);
    HDC hdc = dc.GetSafeHdc();
    if (w <= 1)
    {
        HPEN pen = ::CreatePen(PS_SOLID, 1, c);
        HGDIOBJ op = ::SelectObject(hdc, pen);
        ::MoveToEx(hdc, a.x, a.y, nullptr);
        ::LineTo(hdc, b.x, b.y);
        ::SetPixelV(hdc, b.x, b.y, c);
        ::SelectObject(hdc, op);
        ::DeleteObject(pen);
        return;
    }
    if (a == b)
    {
        HBRUSH br = ::CreateSolidBrush(c);
        HGDIOBJ ob = ::SelectObject(hdc, br);
        HGDIOBJ op = ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
        int r = w / 2;
        ::Ellipse(hdc, a.x - r, a.y - r, a.x - r + w + 1, a.y - r + w + 1);
        ::SelectObject(hdc, op);
        ::SelectObject(hdc, ob);
        ::DeleteObject(br);
        return;
    }
    LOGBRUSH lb = { BS_SOLID, c, 0 };
    HPEN pen = ::ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, w, &lb, 0, nullptr);
    HGDIOBJ op = ::SelectObject(hdc, pen);
    ::MoveToEx(hdc, a.x, a.y, nullptr);
    ::LineTo(hdc, b.x, b.y);
    ::SelectObject(hdc, op);
    ::DeleteObject(pen);
}

void CCanvasWnd::StrokeSegment(CPoint a, CPoint b)
{
    COLORREF c = m_color[m_dragButton];
    int w = GetToolWidth();

    switch (m_tool)
    {
    case TOOL_PENCIL:
        DrawPenLine(a, b, w, c);
        break;
    case TOOL_ERASER:
    {
        DWORD p = ToPixel(m_color[m_dragButton == 0 ? 1 : 0]);
        ::GdiFlush();
        ForLine(a, b, 1, [&](int x, int y)
        {
            m_img.FillRect(CRect(x - w / 2, y - w / 2, x - w / 2 + w, y - w / 2 + w), p);
        });
        break;
    }
    case TOOL_BRUSH:
        switch (m_brush)
        {
        case BRUSH_NORMAL:
            DrawPenLine(a, b, w, c);
            break;
        case BRUSH_CALLI1:
        case BRUSH_CALLI2:
        {
            int h = max(1, w / 2);
            CPoint n = (m_brush == BRUSH_CALLI1) ? CPoint(h, -h) : CPoint(h, h);
            CBitmapDC dc(m_img);
            HDC hdc = dc.GetSafeHdc();
            HPEN pen = ::CreatePen(PS_SOLID, 1, c);
            HBRUSH br = ::CreateSolidBrush(c);
            HGDIOBJ op = ::SelectObject(hdc, pen);
            HGDIOBJ ob = ::SelectObject(hdc, br);
            POINT q[4] = { { a.x + n.x, a.y + n.y }, { b.x + n.x, b.y + n.y },
                           { b.x - n.x, b.y - n.y }, { a.x - n.x, a.y - n.y } };
            ::Polygon(hdc, q, 4);
            ::MoveToEx(hdc, a.x - n.x, a.y - n.y, nullptr);
            ::LineTo(hdc, a.x + n.x, a.y + n.y);
            ::SelectObject(hdc, op);
            ::SelectObject(hdc, ob);
            ::DeleteObject(pen);
            ::DeleteObject(br);
            break;
        }
        case BRUSH_AIR:
            Spray(b);
            break;
        case BRUSH_MARKER:
            ::GdiFlush();
            ForLine(a, b, 1, [&](int x, int y) { MarkerStamp(x, y, w, c); });
            break;
        case BRUSH_CRAYON:
            ::GdiFlush();
            ForLine(a, b, 2, [&](int x, int y) { CrayonStamp(x, y, w, c); });
            break;
        case BRUSH_OIL:
            OilSegment(a, b, w, c);
            break;
        case BRUSH_WATERCOLOR:
            ::GdiFlush();
            ForLine(a, b, max(1, w / 4), [&](int x, int y) { WatercolorStamp(x, y, w, c); });
            break;
        case BRUSH_NATURAL_PENCIL:
            ::GdiFlush();
            ForLine(a, b, 1, [&](int x, int y) { NaturalPencilStamp(x, y, w, c); });
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    m_syncDirty = true;
    Invalidate(FALSE);
}

void CCanvasWnd::Spray(CPoint c)
{
    int r = max(3, GetToolWidth());
    DWORD p = ToPixel(m_color[m_dragButton]);
    int count = max(8, r * r / 5);
    ::GdiFlush();
    for (int i = 0; i < count; ++i)
    {
        int dx = rand() % (2 * r + 1) - r;
        int dy = rand() % (2 * r + 1) - r;
        if (dx * dx + dy * dy > r * r)
            continue;
        int x = c.x + dx, y = c.y + dy;
        if (m_img.InBounds(x, y))
            m_img.SetPixel(x, y, p);
    }
}

// 획 시작 전 그림(실행 취소 스냅숏) 위에 c를 alpha(0~255)만큼 섞는다.
// 같은 획 안에서는 픽셀별 최대 alpha만 반영되므로 겹쳐 칠해도 진해지지 않는다.
void CCanvasWnd::BlendStamp(int x, int y, int alpha, COLORREF c)
{
    if (x < 0 || y < 0 || x >= m_img.Width() || y >= m_img.Height())
        return;
    if (m_undo.empty() || m_strokeAlpha.empty())
        return;
    const CBitmap32& orig = m_undo.back();
    if (orig.Size() != m_img.Size())
        return;
    alpha = ClampV(alpha, 0, 255);
    size_t idx = (size_t)y * m_img.Width() + x;
    if (alpha <= m_strokeAlpha[idx])
        return;
    m_strokeAlpha[idx] = (BYTE)alpha;
    DWORD o = orig.GetPixel(x, y);
    int orr = (o >> 16) & 0xFF, og = (o >> 8) & 0xFF, ob = o & 0xFF;
    int r = orr + (GetRValue(c) - orr) * alpha / 255;
    int g = og + (GetGValue(c) - og) * alpha / 255;
    int b = ob + (GetBValue(c) - ob) * alpha / 255;
    m_img.SetPixel(x, y, 0xFF000000u | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b);
}

void CCanvasWnd::MarkerStamp(int cx, int cy, int w, COLORREF c)
{
    int hw = max(1, w / 4), hh = max(1, w / 2);
    for (int y = cy - hh; y <= cy + hh; ++y)
        for (int x = cx - hw; x <= cx + hw; ++x)
            BlendStamp(x, y, 128, c);
}

// 수채화: 가장자리로 갈수록 옅고, 종이 결이 비치는 반투명 원
void CCanvasWnd::WatercolorStamp(int cx, int cy, int w, COLORREF c)
{
    int r = max(2, w / 2 + 1);
    double r2 = (double)r * r;
    for (int y = cy - r; y <= cy + r; ++y)
    {
        for (int x = cx - r; x <= cx + r; ++x)
        {
            double d2 = (double)(x - cx) * (x - cx) + (double)(y - cy) * (y - cy);
            if (d2 > r2)
                continue;
            double fall = 1.0 - d2 / r2;
            double grain = 0.75 + 0.25 * Grain(x / 2, y / 2);
            BlendStamp(x, y, (int)(105 * (0.35 + 0.65 * fall) * grain), c);
        }
    }
}

// 자연스러운 연필: 종이 결에 따라 흑연이 묻는 거친 선
void CCanvasWnd::NaturalPencilStamp(int cx, int cy, int w, COLORREF c)
{
    double r = max(0.8, w / 2.0);
    int ir = (int)ceil(r);
    for (int y = cy - ir; y <= cy + ir; ++y)
    {
        for (int x = cx - ir; x <= cx + ir; ++x)
        {
            double d = hypot((double)(x - cx), (double)(y - cy));
            if (d > r)
                continue;
            float g = Grain(x, y);
            if (g < 0.30f)
                continue;   // 종이의 오목한 부분은 비워 둔다
            double edge = 1.0 - 0.5 * (d / r);
            BlendStamp(x, y, (int)((90 + 150 * g) * edge), c);
        }
    }
}

// 유화: 붓털마다 밝기가 다른 여러 줄을 진행 방향에 수직으로 나란히 긋는다
void CCanvasWnd::OilSegment(CPoint a, CPoint b, int w, COLORREF c)
{
    if (m_bristles.empty())
        return;
    double dx = b.x - a.x, dy = b.y - a.y;
    double len = hypot(dx, dy);
    if (len >= 1.0)
    {
        // 방향을 부드럽게 바꿔 붓털이 끊기지 않게 한다
        m_oilDirX = m_oilDirX * 0.5 + dx / len * 0.5;
        m_oilDirY = m_oilDirY * 0.5 + dy / len * 0.5;
        double l = hypot(m_oilDirX, m_oilDirY);
        if (l > 1e-6) { m_oilDirX /= l; m_oilDirY /= l; }
    }
    double nx = -m_oilDirY, ny = m_oilDirX;

    CBitmapDC dc(m_img);
    HDC hdc = dc.GetSafeHdc();
    for (const Bristle& br : m_bristles)
    {
        int lift = (int)((br.shade - 1.0) * 120);
        int r = ClampV((int)(GetRValue(c) * br.shade) + lift, 0, 255);
        int g = ClampV((int)(GetGValue(c) * br.shade) + lift, 0, 255);
        int bl = ClampV((int)(GetBValue(c) * br.shade) + lift, 0, 255);
        LOGBRUSH lb = { BS_SOLID, RGB(r, g, bl), 0 };
        HPEN pen = ::ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
                                  max(1, br.width), &lb, 0, nullptr);
        HGDIOBJ op = ::SelectObject(hdc, pen);
        int ax = a.x + (int)lround(nx * br.offset), ay = a.y + (int)lround(ny * br.offset);
        int bx = b.x + (int)lround(nx * br.offset), by = b.y + (int)lround(ny * br.offset);
        ::MoveToEx(hdc, ax, ay, nullptr);
        ::LineTo(hdc, bx == ax && by == ay ? bx + 1 : bx, by);
        ::SelectObject(hdc, op);
        ::DeleteObject(pen);
    }
}

void CCanvasWnd::CrayonStamp(int cx, int cy, int w, COLORREF c)
{
    int r = max(1, w / 2);
    DWORD p = ToPixel(c);
    int count = max(4, r * 3);
    for (int i = 0; i < count; ++i)
    {
        int dx = rand() % (2 * r + 1) - r;
        int dy = rand() % (2 * r + 1) - r;
        if (dx * dx + dy * dy > r * r)
            continue;
        int x = cx + dx, y = cy + dy;
        if (m_img.InBounds(x, y) && ((x * 7 + y * 13) % 5) != 0)
            m_img.SetPixel(x, y, p);
    }
}

bool CCanvasWnd::FloodFill(CPoint pt, COLORREF c)
{
    ::GdiFlush();
    DWORD target = m_img.GetPixel(pt.x, pt.y) & 0x00FFFFFF;
    DWORD fill = ToPixel(c);
    if (target == (fill & 0x00FFFFFF))
        return false;
    PushUndo();
    int W = m_img.Width(), H = m_img.Height();
    std::vector<CPoint> stack;
    stack.push_back(pt);
    while (!stack.empty())
    {
        CPoint p = stack.back();
        stack.pop_back();
        DWORD* row = m_img.Row(p.y);
        if ((row[p.x] & 0x00FFFFFF) != target)
            continue;
        int l = p.x, r = p.x;
        while (l > 0 && (row[l - 1] & 0x00FFFFFF) == target) --l;
        while (r < W - 1 && (row[r + 1] & 0x00FFFFFF) == target) ++r;
        for (int i = l; i <= r; ++i)
            row[i] = fill;
        for (int ny = p.y - 1; ny <= p.y + 1; ny += 2)
        {
            if (ny < 0 || ny >= H) continue;
            DWORD* nr = m_img.Row(ny);
            bool inSpan = false;
            for (int i = l; i <= r; ++i)
            {
                if ((nr[i] & 0x00FFFFFF) == target)
                {
                    if (!inSpan) { stack.push_back(CPoint(i, ny)); inSpan = true; }
                }
                else
                    inSpan = false;
            }
        }
    }
    Invalidate(FALSE);
    return true;
}

// ---------------------------------------------------------------------------
// 도형

// 0/45/90도로 고정
CPoint CCanvasWnd::ConstrainLine(CPoint a, CPoint b) const
{
    int dx = b.x - a.x, dy = b.y - a.y;
    int adx = abs(dx), ady = abs(dy);
    if (adx > 2 * ady) return CPoint(b.x, a.y);
    if (ady > 2 * adx) return CPoint(a.x, b.y);
    int d = max(adx, ady);
    return CPoint(a.x + (dx < 0 ? -d : d), a.y + (dy < 0 ? -d : d));
}

CPoint CCanvasWnd::ConstrainShape(CPoint a, CPoint b) const
{
    if (m_pendingShape == SH_LINE)
        return ConstrainLine(a, b);
    int dx = b.x - a.x, dy = b.y - a.y;
    int d = max(abs(dx), abs(dy));
    return CPoint(a.x + (dx < 0 ? -d : d), a.y + (dy < 0 ? -d : d));
}

CRect CCanvasWnd::ResizeRectByHandle(const CRect& r0, int h, CPoint ip) const
{
    CRect r = r0;
    switch (h)
    {
    case 0: r.left = ip.x; r.top = ip.y; break;
    case 1: r.top = ip.y; break;
    case 2: r.right = ip.x + 1; r.top = ip.y; break;
    case 3: r.right = ip.x + 1; break;
    case 4: r.right = ip.x + 1; r.bottom = ip.y + 1; break;
    case 5: r.bottom = ip.y + 1; break;
    case 6: r.left = ip.x; r.bottom = ip.y + 1; break;
    case 7: r.left = ip.x; break;
    }
    r.NormalizeRect();
    if (r.Width() < 1) r.right = r.left + 1;
    if (r.Height() < 1) r.bottom = r.top + 1;
    return r;
}

void CCanvasWnd::DrawPendingShape(CDC* dc)
{
    COLORREF outC = m_shapeSwap ? m_color[1] : m_color[0];
    COLORREF fillC = m_shapeSwap ? m_color[0] : m_color[1];
    DrawShape(dc, m_pendingShape, m_sp1, m_sp2, ShapeWidth(), outC, fillC, m_outline, m_fillShape);
}

void CCanvasWnd::DrawCurve(CDC* dc)
{
    DrawBezierCurve(dc, m_cv, ShapeWidth(), m_color[m_curveSwap ? 1 : 0]);
}

void CCanvasWnd::DrawPolygonPreview(CDC* dc)
{
    if (m_polyPts.empty())
        return;
    std::vector<POINT> pts(m_polyPts.begin(), m_polyPts.end());
    if (m_polyCursor != m_polyPts.back())
        pts.push_back(m_polyCursor);
    COLORREF outC = m_color[m_polySwap ? 1 : 0];
    DrawPolyShape(dc, pts, false, ShapeWidth(), outC, outC, true, false);
}

// 확정 전 도형·곡선·다각형을 모두 그림에 반영한다
void CCanvasWnd::CommitShape()
{
    CommitCurve();
    CommitPolygon();
    if (!m_shapePending)
        return;
    m_shapePending = false;
    PushUndo();
    {
        CBitmapDC dc(m_img);
        DrawPendingShape(&dc.DC());
    }
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::CommitCurve()
{
    if (m_curveStage == 0)
        return;
    m_curveStage = 0;
    PushUndo();
    {
        CBitmapDC dc(m_img);
        DrawCurve(&dc.DC());
    }
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::CommitPolygon()
{
    if (!m_polyActive)
        return;
    m_polyActive = false;
    std::vector<POINT> pts(m_polyPts.begin(), m_polyPts.end());
    m_polyPts.clear();
    if (pts.size() >= 2)
    {
        PushUndo();
        CBitmapDC dc(m_img);
        COLORREF outC = m_color[m_polySwap ? 1 : 0];
        COLORREF fillC = m_color[m_polySwap ? 0 : 1];
        DrawPolyShape(&dc.DC(), pts, true, ShapeWidth(), outC, fillC, m_outline, m_fillShape);
    }
    Invalidate(FALSE);
    NotifyState();
}

// ---------------------------------------------------------------------------
// 선택 영역

const CBitmap32& CCanvasWnd::FloatScaled()
{
    if (m_float.IsNull())
        return m_float;
    int w = max(1, m_selRect.Width()), h = max(1, m_selRect.Height());
    if (m_float.Width() == w && m_float.Height() == h)
        return m_float;
    if (m_floatDirty || m_floatCache.Width() != w || m_floatCache.Height() != h)
    {
        m_floatCache = m_float.Scale(w, h);
        m_floatDirty = false;
    }
    return m_floatCache;
}

void CCanvasWnd::BakeFloat()
{
    if (m_float.IsNull())
        return;
    if (m_float.Width() != m_selRect.Width() || m_float.Height() != m_selRect.Height())
    {
        CBitmap32 b = FloatScaled();
        m_float = std::move(b);
        m_floatDirty = true;
    }
}

void CCanvasWnd::CompositeFloat(CBitmap32& dst, int ox, int oy)
{
    const CBitmap32& f = FloatScaled();
    if (f.IsNull() || dst.IsNull())
        return;
    ::GdiFlush();
    DWORD key = ToPixel(m_color[1]) & 0x00FFFFFF;
    bool transp = m_transparentSel;
    int x0 = max(0, -ox), y0 = max(0, -oy);
    int x1 = min(f.Width(), dst.Width() - ox), y1 = min(f.Height(), dst.Height() - oy);
    for (int y = y0; y < y1; ++y)
    {
        const DWORD* s = f.Row(y);
        DWORD* d = dst.Row(y + oy);
        for (int x = x0; x < x1; ++x)
        {
            DWORD p = s[x];
            if ((p >> 24) < 128)
                continue;
            if (transp && (p & 0x00FFFFFF) == key)
                continue;
            d[x + ox] = p | 0xFF000000u;
        }
    }
}

void CCanvasWnd::ApplyFreeMask(CBitmap32& b)
{
    CBitmap32 mask;
    if (!mask.Create(b.Width(), b.Height(), 0))
        return;
    {
        CBitmapDC dc(mask);
        HDC hdc = dc.GetSafeHdc();
        std::vector<POINT> pts;
        for (auto& p : m_freePoly)
            pts.push_back({ p.x - m_selRect.left, p.y - m_selRect.top });
        HGDIOBJ ob = ::SelectObject(hdc, ::GetStockObject(WHITE_BRUSH));
        HGDIOBJ op = ::SelectObject(hdc, ::GetStockObject(WHITE_PEN));
        ::SetPolyFillMode(hdc, WINDING);
        ::Polygon(hdc, pts.data(), (int)pts.size());
        ::SelectObject(hdc, op);
        ::SelectObject(hdc, ob);
    }
    ::GdiFlush();
    for (int y = 0; y < b.Height(); ++y)
    {
        const DWORD* m = mask.Row(y);
        DWORD* d = b.Row(y);
        for (int x = 0; x < b.Width(); ++x)
            d[x] = (d[x] & 0x00FFFFFF) | ((m[x] & 0x00FFFFFF) ? 0xFF000000u : 0);
    }
}

void CCanvasWnd::Lift()
{
    if (!m_selActive || m_lifted)
        return;
    PushUndo();
    m_float = m_img.Crop(m_selRect, 0xFFFFFFFF);
    if (m_selIsFree)
        ApplyFreeMask(m_float);
    else
        m_float.SetAlphaAll(255);

    DWORD bg = ToPixel(m_color[1]);
    ::GdiFlush();
    for (int y = 0; y < m_float.Height(); ++y)
    {
        int iy = m_selRect.top + y;
        if (iy < 0 || iy >= m_img.Height()) continue;
        const DWORD* s = m_float.Row(y);
        DWORD* d = m_img.Row(iy);
        for (int x = 0; x < m_float.Width(); ++x)
        {
            int ix = m_selRect.left + x;
            if (ix < 0 || ix >= m_img.Width()) continue;
            if (s[x] >> 24)
                d[ix] = bg;
        }
    }
    m_lifted = true;
    m_floatDirty = true;
}

void CCanvasWnd::ClearSelection()
{
    m_selActive = false;
    m_lifted = false;
    m_selIsFree = false;
    m_float.Destroy();
    m_floatCache.Destroy();
    m_freePoly.clear();
}

void CCanvasWnd::CommitSelection()
{
    if (!m_selActive)
        return;
    if (m_lifted)
        CompositeFloat(m_img, m_selRect.left, m_selRect.top);
    m_syncDirty = true;
    ClearSelection();
    Invalidate(FALSE);
    UpdateStatus();
    NotifyState();
}

bool CCanvasWnd::GetSelectionImage(CBitmap32& out)
{
    if (!m_selActive)
        return false;
    if (m_lifted)
    {
        out = FloatScaled();
    }
    else
    {
        out = m_img.Crop(m_selRect, 0xFFFFFFFF);
        if (m_selIsFree)
            ApplyFreeMask(out);
        else
            out.SetAlphaAll(255);
    }
    return !out.IsNull();
}

void CCanvasWnd::ResizeCanvas(int w, int h)
{
    CBitmap32 n = m_img.Crop(CRect(0, 0, w, h), 0xFFFFFFFF);
    if (!n.IsNull())
        m_img = std::move(n);
}

// ---------------------------------------------------------------------------
// 텍스트

bool CCanvasWnd::IsTextEditFocused() const
{
    return m_textActive && m_edit.GetSafeHwnd() && ::GetFocus() == m_edit.GetSafeHwnd();
}

void CCanvasWnd::EndTextEdit(bool commit)
{
    if (commit)
        CommitText();
    else
        CancelText();
}

void CCanvasWnd::FocusDefault()
{
    if (m_textActive && m_edit.GetSafeHwnd())
        m_edit.SetFocus();
    else if (GetSafeHwnd())
        SetFocus();
}

void CCanvasWnd::BeginTextEdit(const CRect& rc)
{
    m_textRect = rc;
    m_textActive = true;
    if (!m_edit.GetSafeHwnd())
        m_edit.Create(WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_LEFT,
                      CRect(0, 0, 10, 10), this, IDC_TEXTEDIT);
    m_edit.SetWindowText(_T(""));
    UpdateTextEditFont();
    UpdateTextEditPos();
    m_edit.ShowWindow(SW_SHOW);
    m_edit.SetFocus();
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::CancelText()
{
    if (!m_textActive)
        return;
    m_textActive = false;
    if (m_edit.GetSafeHwnd())
    {
        m_edit.ShowWindow(SW_HIDE);
        m_edit.SetWindowText(_T(""));
    }
    if (GetSafeHwnd())
    {
        SetFocus();
        Invalidate(FALSE);
    }
    NotifyState();
}

void CCanvasWnd::CommitText()
{
    if (!m_textActive)
        return;
    m_textActive = false;
    CString s;
    m_edit.GetWindowText(s);
    m_edit.ShowWindow(SW_HIDE);
    m_edit.SetWindowText(_T(""));
    if (!s.IsEmpty())
    {
        PushUndo();
        CBitmapDC dc(m_img);
        CFont f;
        LOGFONT lf = m_lf;
        lf.lfHeight = -PtToPx(m_fontPt);
        lf.lfQuality = ANTIALIASED_QUALITY;
        f.CreateFontIndirect(&lf);
        CFont* of = dc->SelectObject(&f);
        if (m_textOpaque)
            dc->FillSolidRect(m_textRect, m_color[1]);
        dc->SetTextColor(m_color[0]);
        dc->SetBkMode(TRANSPARENT);
        CRect r = m_textRect;
        dc->DrawText(s, r, DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL | DT_LEFT | DT_TOP);
        dc->SelectObject(of);
    }
    SetFocus();
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::UpdateTextEditFont()
{
    if (!m_edit.GetSafeHwnd())
        return;
    LOGFONT lf = m_lf;
    lf.lfHeight = -max(1, (int)lround(PtToPx(m_fontPt) * m_zoom));
    lf.lfQuality = ANTIALIASED_QUALITY;
    CFont nf;
    nf.CreateFontIndirect(&lf);
    m_edit.SetFont(&nf);
    m_edit.SetMargins(0, 0);
    m_editFont.DeleteObject();
    m_editFont.Attach(nf.Detach());
    m_edit.Invalidate();
}

void CCanvasWnd::UpdateTextEditPos()
{
    if (!m_textActive || !m_edit.GetSafeHwnd())
        return;
    CRect s = ImgRectToScreen(m_textRect);
    m_edit.MoveWindow(s);
}

void CCanvasWnd::OnTextChange()
{
    if (!m_textActive)
        return;
    // 내용이 넘치면 텍스트 상자를 아래로 늘린다
    CString s;
    m_edit.GetWindowText(s);
    if (s.IsEmpty() || s.Right(1) == _T("\n"))
        s += _T("Ag");
    CClientDC cdc(this);
    CFont f;
    LOGFONT lf = m_lf;
    lf.lfHeight = -PtToPx(m_fontPt);
    f.CreateFontIndirect(&lf);
    CFont* of = cdc.SelectObject(&f);
    CRect r(0, 0, m_textRect.Width(), 0);
    cdc.DrawText(s, r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
    cdc.SelectObject(of);
    int need = r.Height() + 4;
    if (need > m_textRect.Height())
    {
        m_textRect.bottom = m_textRect.top + need;
        UpdateTextEditPos();
        Invalidate(FALSE);
        UpdateStatus();
    }
}

HBRUSH CCanvasWnd::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (pWnd && pWnd->GetSafeHwnd() == m_edit.GetSafeHwnd())
    {
        COLORREF bk = m_textOpaque ? m_color[1] : RGB(255, 255, 255);
        pDC->SetTextColor(m_color[0]);
        pDC->SetBkColor(bk);
        if (m_editBrushColor != bk)
        {
            m_editBrush.DeleteObject();
            m_editBrush.CreateSolidBrush(bk);
            m_editBrushColor = bk;
        }
        return (HBRUSH)m_editBrush.GetSafeHandle();
    }
    return CWnd::OnCtlColor(pDC, pWnd, nCtlColor);
}
