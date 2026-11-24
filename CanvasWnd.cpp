// CanvasWnd.cpp : 캔버스 창 - 보기(확대/스크롤), 그리기, 실행 취소, 파일 입출력
#include "framework.h"
#include "resource.h"
#include "CanvasWnd.h"
#include "MainFrm.h"
#include "Theme.h"

const TCHAR* g_brushNames[BRUSH_COUNT] =
{
    _T("브러시"), _T("캘리그라피 브러시 1"), _T("캘리그라피 브러시 2"),
    _T("에어브러시"), _T("유화 브러시"), _T("크레용"), _T("마커"),
    _T("자연스러운 연필"), _T("수채화 브러시")
};
const int g_fontSizes[FONT_SIZE_COUNT] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 36, 48, 72 };

static const double kZoomLevels[] = { 0.125, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0 };
static const int kZoomCount = sizeof(kZoomLevels) / sizeof(kZoomLevels[0]);
static const int MARGIN = 20;
static const size_t MAX_UNDO = 40;

BEGIN_MESSAGE_MAP(CCanvasWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_HSCROLL()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_SETCURSOR()
    ON_WM_KEYDOWN()
    ON_WM_TIMER()
    ON_WM_CAPTURECHANGED()
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(IDC_TEXTEDIT, &CCanvasWnd::OnTextChange)

    ON_COMMAND_RANGE(ID_TOOL_SELECT_RECT, ID_TOOL_BRUSH, &CCanvasWnd::OnToolCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_TOOL_SELECT_RECT, ID_TOOL_BRUSH, &CCanvasWnd::OnUpdateToolCmd)
    ON_COMMAND_RANGE(ID_SHAPE_FIRST, ID_SHAPE_LAST, &CCanvasWnd::OnShapeCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_SHAPE_FIRST, ID_SHAPE_LAST, &CCanvasWnd::OnUpdateShapeCmd)
    ON_COMMAND_RANGE(ID_BRUSH_FIRST, ID_BRUSH_LAST, &CCanvasWnd::OnBrushCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_BRUSH_FIRST, ID_BRUSH_LAST, &CCanvasWnd::OnUpdateBrushCmd)
    ON_COMMAND_RANGE(ID_SIZE_INC, ID_SIZE_DEC, &CCanvasWnd::OnSizeCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_SIZE_INC, ID_SIZE_DEC, &CCanvasWnd::OnUpdateSizeCmd)
    ON_COMMAND_RANGE(ID_OUTLINE_NONE, ID_FILL_SOLID, &CCanvasWnd::OnShapeModeCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_OUTLINE_NONE, ID_FILL_SOLID, &CCanvasWnd::OnUpdateShapeModeCmd)

    ON_COMMAND(ID_EDIT_UNDO, &CCanvasWnd::OnEditUndo)
    ON_COMMAND(ID_EDIT_REDO, &CCanvasWnd::OnEditRedo)
    ON_COMMAND(ID_EDIT_CUT, &CCanvasWnd::OnEditCut)
    ON_COMMAND(ID_EDIT_COPY, &CCanvasWnd::OnEditCopy)
    ON_COMMAND(ID_EDIT_PASTE, &CCanvasWnd::OnEditPaste)
    ON_COMMAND(ID_EDIT_SELECT_ALL, &CCanvasWnd::OnEditSelectAll)
    ON_COMMAND(ID_EDIT_CLEAR, &CCanvasWnd::OnEditClear)
    ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, &CCanvasWnd::OnUpdateEditUndo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, &CCanvasWnd::OnUpdateEditRedo)
    ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, &CCanvasWnd::OnUpdateEditCutCopy)
    ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, &CCanvasWnd::OnUpdateEditCutCopy)
    ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, &CCanvasWnd::OnUpdateEditPaste)
    ON_UPDATE_COMMAND_UI(ID_EDIT_CLEAR, &CCanvasWnd::OnUpdateEditClear)

    ON_COMMAND_RANGE(ID_IMAGE_CROP, ID_IMAGE_CLEAR, &CCanvasWnd::OnImageCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_IMAGE_CROP, ID_IMAGE_CLEAR, &CCanvasWnd::OnUpdateImageCmd)
    ON_COMMAND(ID_SELECT_TRANSPARENT, &CCanvasWnd::OnSelectTransparent)
    ON_UPDATE_COMMAND_UI(ID_SELECT_TRANSPARENT, &CCanvasWnd::OnUpdateSelectTransparent)
    ON_COMMAND_RANGE(ID_VIEW_ZOOM_IN, ID_VIEW_ZOOM_FIT, &CCanvasWnd::OnZoomCmd)
    ON_COMMAND(ID_VIEW_GRID, &CCanvasWnd::OnViewGrid)
    ON_UPDATE_COMMAND_UI(ID_VIEW_GRID, &CCanvasWnd::OnUpdateViewGrid)
    ON_COMMAND(ID_VIEW_RULERS, &CCanvasWnd::OnViewRulers)
    ON_UPDATE_COMMAND_UI(ID_VIEW_RULERS, &CCanvasWnd::OnUpdateViewRulers)
    ON_COMMAND(ID_COLOR_EDIT, &CCanvasWnd::OnColorEdit)
    ON_COMMAND(ID_COLOR_SWAP, &CCanvasWnd::OnColorSwap)
    ON_COMMAND_RANGE(ID_TEXT_FONT, ID_TEXT_OPAQUE, &CCanvasWnd::OnTextCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_TEXT_FONT, ID_TEXT_OPAQUE, &CCanvasWnd::OnUpdateTextCmd)
    ON_COMMAND_RANGE(ID_FONTSIZE_FIRST, ID_FONTSIZE_LAST, &CCanvasWnd::OnFontSizeCmd)
    ON_UPDATE_COMMAND_UI_RANGE(ID_FONTSIZE_FIRST, ID_FONTSIZE_LAST, &CCanvasWnd::OnUpdateFontSizeCmd)
END_MESSAGE_MAP()

CCanvasWnd::CCanvasWnd()
{
    m_color[0] = RGB(0, 0, 0);
    m_color[1] = RGB(255, 255, 255);
    m_toolSize[SC_PENCIL] = 1;
    m_toolSize[SC_BRUSH] = 8;
    m_toolSize[SC_ERASER] = 10;
    m_toolSize[SC_SHAPE] = 3;
    m_cv[0] = m_cv[1] = m_cv[2] = m_cv[3] = POINT{ 0, 0 };
    for (auto& c : m_custom)
        c = CLR_INVALID;
    memset(&m_lf, 0, sizeof(m_lf));
    m_lf.lfHeight = -PtToPx(m_fontPt);
    m_lf.lfWeight = FW_NORMAL;
    m_lf.lfCharSet = DEFAULT_CHARSET;
    m_lf.lfQuality = ANTIALIASED_QUALITY;
    _tcscpy_s(m_lf.lfFaceName, _T("맑은 고딕"));
    srand(::GetTickCount());
}

CCanvasWnd::~CCanvasWnd()
{
}

BOOL CCanvasWnd::Create(CWnd* pParent, UINT nID)
{
    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!CWnd::CreateEx(0, cls, _T(""), WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | WS_CLIPCHILDREN,
                        CRect(0, 0, 100, 100), pParent, nID))
        return FALSE;

    CClientDC dc(this);
    m_dpi = dc.GetDeviceCaps(LOGPIXELSY);
    m_rulerSize = MulDiv(22, m_dpi, 96);
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(8, m_dpi, 72);
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    m_rulerFontH.CreateFontIndirect(&lf);
    lf.lfEscapement = lf.lfOrientation = 900;   // 세로 눈금자: 90도 회전
    m_rulerFontV.CreateFontIndirect(&lf);
    m_rulers = AfxGetApp()->GetProfileInt(_T("Settings"), _T("Rulers"), 0) != 0;

    ApplyTheme();
    NewImage(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    return TRUE;
}

CMainFrame* CCanvasWnd::Frame() const
{
    return static_cast<CMainFrame*>(GetParent());
}

void CCanvasWnd::ApplyTheme()
{
    if (!GetSafeHwnd())
        return;
    ApplyThemeToControl(m_hWnd);     // 스크롤 막대
    SetWindowPos(nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    Invalidate(FALSE);
}

SizeCategory CCanvasWnd::SizeCategoryOf(PaintTool t)
{
    switch (t)
    {
    case TOOL_PENCIL: return SC_PENCIL;
    case TOOL_BRUSH:  return SC_BRUSH;
    case TOOL_ERASER: return SC_ERASER;
    default:          return SC_SHAPE;
    }
}

bool CCanvasWnd::IsSizeEnabled() const
{
    return m_tool == TOOL_PENCIL || m_tool == TOOL_BRUSH || m_tool == TOOL_ERASER || m_tool == TOOL_SHAPE;
}

void CCanvasWnd::SetToolSize(int size)
{
    size = ClampV(size, MIN_TOOL_SIZE, MAX_TOOL_SIZE);
    int& cur = m_toolSize[SizeCategoryOf(m_tool)];
    if (cur == size)
        return;
    cur = size;
    Invalidate(FALSE);     // 확정 전 도형과 지우개 미리보기 갱신
    NotifyState();
}

// ---------------------------------------------------------------------------
// 좌표 변환

CRect CCanvasWnd::ViewRect() const
{
    CRect cl;
    GetClientRect(&cl);
    if (m_rulers)
    {
        cl.left = min(cl.right, cl.left + m_rulerSize);
        cl.top = min(cl.bottom, cl.top + m_rulerSize);
    }
    return cl;
}

CSize CCanvasWnd::ZoomedSize() const
{
    return CSize(max(1, Z(m_img.Width())), max(1, Z(m_img.Height())));
}

CPoint CCanvasWnd::Origin() const
{
    CRect v = ViewRect();
    CSize z = ZoomedSize();
    int x = (z.cx + 2 * MARGIN <= v.Width()) ? v.left + (v.Width() - z.cx) / 2 : v.left + MARGIN - m_scroll.x;
    int y = (z.cy + 2 * MARGIN <= v.Height()) ? v.top + (v.Height() - z.cy) / 2 : v.top + MARGIN - m_scroll.y;
    return CPoint(x, y);
}

CPoint CCanvasWnd::ScreenToImg(CPoint p) const
{
    CPoint o = Origin();
    return CPoint((int)floor((p.x - o.x) / m_zoom), (int)floor((p.y - o.y) / m_zoom));
}

CPoint CCanvasWnd::ImgToScreen(int x, int y) const
{
    CPoint o = Origin();
    return CPoint(o.x + Z(x), o.y + Z(y));
}

CRect CCanvasWnd::ImgRectToScreen(const CRect& r) const
{
    CPoint a = ImgToScreen(r.left, r.top);
    CPoint b = ImgToScreen(r.right, r.bottom);
    return CRect(a, b);
}

// ---------------------------------------------------------------------------
// 스크롤/확대

void CCanvasWnd::UpdateScrollBars()
{
    if (!GetSafeHwnd() || m_inScrollUpdate)
        return;
    m_inScrollUpdate = true;
    for (int pass = 0; pass < 2; ++pass)
    {
        CRect cl = ViewRect();
        CSize z = ZoomedSize();
        int cw = z.cx + 2 * MARGIN, ch = z.cy + 2 * MARGIN;
        m_scroll.x = ClampV<int>(m_scroll.x, 0, max(0, cw - cl.Width()));
        m_scroll.y = ClampV<int>(m_scroll.y, 0, max(0, ch - cl.Height()));

        SCROLLINFO si = { sizeof(SCROLLINFO) };
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = cw - 1;
        si.nPage = (UINT)max(0, (int)cl.Width());
        si.nPos = m_scroll.x;
        SetScrollInfo(SB_HORZ, &si, TRUE);

        si.nMax = ch - 1;
        si.nPage = (UINT)max(0, (int)cl.Height());
        si.nPos = m_scroll.y;
        SetScrollInfo(SB_VERT, &si, TRUE);
    }
    m_inScrollUpdate = false;
    UpdateTextEditPos();
}

void CCanvasWnd::ScrollTo(int x, int y)
{
    m_scroll = CPoint(x, y);
    UpdateScrollBars();
    Invalidate(FALSE);
}

void CCanvasWnd::SetZoom(double z, CPoint anchor)
{
    z = ClampV(z, 0.05, 8.0);
    if (fabs(z - m_zoom) < 1e-9)
        return;
    CPoint o = Origin();
    double ix = (anchor.x - o.x) / m_zoom;
    double iy = (anchor.y - o.y) / m_zoom;
    m_zoom = z;
    CRect v = ViewRect();
    m_scroll.x = v.left + MARGIN + (int)(ix * z) - anchor.x;
    m_scroll.y = v.top + MARGIN + (int)(iy * z) - anchor.y;
    UpdateScrollBars();
    UpdateTextEditFont();
    UpdateTextEditPos();
    Invalidate(FALSE);
    UpdateStatus();
}

void CCanvasWnd::SetZoomCentered(double z)
{
    SetZoom(z, ViewRect().CenterPoint());
}

void CCanvasWnd::ZoomStepCentered(int dir)
{
    ZoomStep(dir, ViewRect().CenterPoint());
}

void CCanvasWnd::ZoomStep(int dir, CPoint anchor)
{
    double target = m_zoom;
    if (dir > 0)
    {
        for (int i = 0; i < kZoomCount; ++i)
            if (kZoomLevels[i] > m_zoom + 1e-6) { target = kZoomLevels[i]; break; }
    }
    else
    {
        for (int i = kZoomCount - 1; i >= 0; --i)
            if (kZoomLevels[i] < m_zoom - 1e-6) { target = kZoomLevels[i]; break; }
    }
    SetZoom(target, anchor);
}

void CCanvasWnd::OnImageSizeChanged()
{
    m_floatDirty = true;
    UpdateScrollBars();
    Invalidate(FALSE);
    UpdateStatus();
    NotifyState();
}

void CCanvasWnd::UpdateStatus()
{
    CMainFrame* f = Frame();
    if (!f || !f->GetSafeHwnd())
        return;
    CString pos, sel, size, zoom;
    if (m_hoverIn)
        pos.Format(_T("%d, %dpx"), m_hoverImg.x, m_hoverImg.y);
    if (m_selActive || m_drag == DRAG_SEL_NEW)
        sel.Format(_T("%d × %dpx"), m_selRect.Width(), m_selRect.Height());
    else if (m_curveStage > 0 || m_drag == DRAG_CURVE)
        sel.Format(_T("%d × %dpx"), abs(m_cv[3].x - m_cv[0].x) + 1, abs(m_cv[3].y - m_cv[0].y) + 1);
    else if (m_shapePending || m_drag == DRAG_SHAPE_NEW)
        sel.Format(_T("%d × %dpx"), abs(m_sp2.x - m_sp1.x) + 1, abs(m_sp2.y - m_sp1.y) + 1);
    else if (m_drag == DRAG_TEXT_NEW || m_textActive)
        sel.Format(_T("%d × %dpx"), m_textRect.Width(), m_textRect.Height());
    if (m_drag == DRAG_CANVAS)
        size.Format(_T("%d × %dpx"), m_canvasNewSize.cx, m_canvasNewSize.cy);
    else
        size.Format(_T("%d × %dpx"), m_img.Width(), m_img.Height());
    zoom.Format(_T("%d%%"), (int)lround(m_zoom * 100));
    f->SetStatusTexts(pos, sel, size, zoom, m_zoom);
}

void CCanvasWnd::NotifyState()
{
    CMainFrame* f = Frame();
    if (f && f->GetSafeHwnd())
        f->OnCanvasStateChanged();
}

// ---------------------------------------------------------------------------
// 실행 취소

void CCanvasWnd::PushUndo()
{
    m_undo.push_back(m_img);
    if (m_undo.size() > MAX_UNDO)
        m_undo.erase(m_undo.begin());
    m_redo.clear();
    SetModified(true);
}

void CCanvasWnd::Undo()
{
    if (m_textActive)
    {
        CancelText();
        return;
    }
    if (IsShapeBusy())
    {
        // 확정 전 도형/곡선/다각형은 버린다
        m_shapePending = false;
        m_curveStage = 0;
        m_polyActive = false;
        m_polyPts.clear();
        Invalidate(FALSE);
        NotifyState();
        return;
    }
    if (m_selActive)
        ClearSelection();
    if (m_undo.empty())
    {
        Invalidate(FALSE);
        NotifyState();
        return;
    }
    m_redo.push_back(std::move(m_img));
    m_img = std::move(m_undo.back());
    m_undo.pop_back();
    SetModified(true);
    OnImageSizeChanged();
}

void CCanvasWnd::Redo()
{
    if (m_redo.empty())
        return;
    CancelText();
    m_shapePending = false;
    m_curveStage = 0;
    m_polyActive = false;
    m_polyPts.clear();
    ClearSelection();
    m_undo.push_back(std::move(m_img));
    m_img = std::move(m_redo.back());
    m_redo.pop_back();
    SetModified(true);
    OnImageSizeChanged();
}

void CCanvasWnd::SetModified(bool b)
{
    if (m_modified != b)
    {
        m_modified = b;
        CMainFrame* f = Frame();
        if (f && f->GetSafeHwnd())
            f->UpdateTitle();
    }
}

void CCanvasWnd::FlattenPending()
{
    CommitText();
    CommitShape();
    CommitSelection();
}

// ---------------------------------------------------------------------------
// 파일

void CCanvasWnd::NewImage(int w, int h)
{
    CancelText();
    m_shapePending = false;
    m_curveStage = 0;
    m_polyActive = false;
    m_polyPts.clear();
    ClearSelection();
    m_img.Create(w, h, 0xFFFFFFFF);
    m_undo.clear();
    m_redo.clear();
    m_zoom = 1.0;
    m_scroll = CPoint(0, 0);
    SetModified(false);
    OnImageSizeChanged();
}

bool CCanvasWnd::LoadFile(LPCTSTR path, CString& err)
{
    CImage img;
    if (FAILED(img.Load(path)))
    {
        err.Format(_T("'%s' 파일을 열 수 없습니다.\n지원되지 않는 형식이거나 손상된 파일입니다."), path);
        return false;
    }
    CBitmap32 b;
    if (!b.Create(img.GetWidth(), img.GetHeight(), 0xFFFFFFFF))
    {
        err = _T("이미지를 불러오기 위한 메모리가 부족합니다.");
        return false;
    }

    if (img.GetBPP() == 32)
    {
        // 알파 채널을 흰색 배경 위에 합성한다. 알파가 모두 0이면 불투명으로 취급.
        bool allZero = true;
        for (int y = 0; y < img.GetHeight() && allZero; ++y)
        {
            const BYTE* row = (const BYTE*)img.GetPixelAddress(0, y);
            for (int x = 0; x < img.GetWidth(); ++x)
                if (row[x * 4 + 3] != 0) { allZero = false; break; }
        }
        ::GdiFlush();
        for (int y = 0; y < img.GetHeight(); ++y)
        {
            const BYTE* row = (const BYTE*)img.GetPixelAddress(0, y);
            DWORD* d = b.Row(y);
            for (int x = 0; x < img.GetWidth(); ++x)
            {
                int bl = row[x * 4], g = row[x * 4 + 1], r = row[x * 4 + 2];
                int a = allZero ? 255 : row[x * 4 + 3];
                r = (r * a + 255 * (255 - a)) / 255;
                g = (g * a + 255 * (255 - a)) / 255;
                bl = (bl * a + 255 * (255 - a)) / 255;
                d[x] = 0xFF000000u | (r << 16) | (g << 8) | bl;
            }
        }
    }
    else
    {
        CBitmapDC dc(b);
        img.BitBlt(dc.GetSafeHdc(), 0, 0, SRCCOPY);
    }

    CancelText();
    m_shapePending = false;
    m_curveStage = 0;
    m_polyActive = false;
    m_polyPts.clear();
    ClearSelection();
    m_img = std::move(b);
    m_undo.clear();
    m_redo.clear();
    m_scroll = CPoint(0, 0);
    m_zoom = 1.0;
    // 창보다 크면 창에 맞춘다
    CRect cl = ViewRect();
    if (cl.Width() > 2 * MARGIN && cl.Height() > 2 * MARGIN &&
        (m_img.Width() > cl.Width() - 2 * MARGIN || m_img.Height() > cl.Height() - 2 * MARGIN))
    {
        double z = min((double)(cl.Width() - 2 * MARGIN) / m_img.Width(),
                       (double)(cl.Height() - 2 * MARGIN) / m_img.Height());
        m_zoom = ClampV(z, 0.05, 1.0);
    }
    SetModified(false);
    UpdateTextEditFont();
    OnImageSizeChanged();
    return true;
}

bool CCanvasWnd::SaveFile(LPCTSTR path, CString& err)
{
    FlattenPending();

    CString p(path);
    CString ext;
    int dot = p.ReverseFind(_T('.'));
    if (dot >= 0)
        ext = p.Mid(dot).MakeLower();
    GUID fmt = Gdiplus::ImageFormatPNG;
    if (ext == _T(".jpg") || ext == _T(".jpeg") || ext == _T(".jpe") || ext == _T(".jfif"))
        fmt = Gdiplus::ImageFormatJPEG;
    else if (ext == _T(".bmp") || ext == _T(".dib"))
        fmt = Gdiplus::ImageFormatBMP;
    else if (ext == _T(".gif"))
        fmt = Gdiplus::ImageFormatGIF;
    else if (ext == _T(".tif") || ext == _T(".tiff"))
        fmt = Gdiplus::ImageFormatTIFF;

    CImage img;
    if (!img.Create(m_img.Width(), m_img.Height(), 24))
    {
        err = _T("저장할 이미지를 만들 수 없습니다.");
        return false;
    }
    {
        HDC hdc = img.GetDC();
        CBitmapDC src(m_img);
        ::BitBlt(hdc, 0, 0, m_img.Width(), m_img.Height(), src.GetSafeHdc(), 0, 0, SRCCOPY);
        img.ReleaseDC();
    }
    if (FAILED(img.Save(path, fmt)))
    {
        err.Format(_T("'%s' 파일을 저장할 수 없습니다."), path);
        return false;
    }
    SetModified(false);
    return true;
}

// ---------------------------------------------------------------------------
// 그리기

BOOL CCanvasWnd::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CCanvasWnd::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UpdateScrollBars();
    Invalidate(FALSE);
}

void CCanvasWnd::Compose(CBitmap32& out, const CRect& vis)
{
    if (!out.Create(vis.Width(), vis.Height(), 0xFFFFFFFF))
        return;
    ::GdiFlush();
    for (int y = vis.top; y < vis.bottom; ++y)
        memcpy(out.Row(y - vis.top), m_img.Row(y) + vis.left, (size_t)vis.Width() * 4);

    if (m_selActive && m_lifted)
        CompositeFloat(out, m_selRect.left - vis.left, m_selRect.top - vis.top);

    if (m_shapePending || m_drag == DRAG_SHAPE_NEW || m_curveStage > 0 || m_drag == DRAG_CURVE || m_polyActive)
    {
        CBitmapDC dc(out);
        ::SetViewportOrgEx(dc.GetSafeHdc(), -vis.left, -vis.top, nullptr);
        if (m_shapePending || m_drag == DRAG_SHAPE_NEW)
            DrawPendingShape(&dc.DC());
        if (m_curveStage > 0 || m_drag == DRAG_CURVE)
            DrawCurve(&dc.DC());
        if (m_polyActive)
            DrawPolygonPreview(&dc.DC());
    }
}

void CCanvasWnd::OnPaint()
{
    CPaintDC pdc(this);
    CRect cl;
    GetClientRect(&cl);
    if (cl.IsRectEmpty())
        return;

    CDC mem;
    mem.CreateCompatibleDC(&pdc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&pdc, cl.Width(), cl.Height());
    CBitmap* oldBmp = mem.SelectObject(&bmp);
    const ThemeColors& th = Theme();
    mem.FillSolidRect(cl, th.canvasBg);

    if (!m_img.IsNull())
    {
        CPoint o = Origin();
        CSize z = ZoomedSize();
        CRect imgScr(o, z);
        CRect shadow = imgScr;
        shadow.OffsetRect(2, 2);
        mem.FillSolidRect(shadow, th.shadow);

        // 다시 그려야 하는 영역의 이미지 부분만 합성한다
        CRect view = ViewRect();
        CRect need;
        need.IntersectRect(view, &pdc.m_ps.rcPaint);
        CRect visScr;
        if (visScr.IntersectRect(imgScr, need))
        {
            CRect vis((int)floor((visScr.left - o.x) / m_zoom), (int)floor((visScr.top - o.y) / m_zoom),
                      (int)ceil((visScr.right - o.x) / m_zoom), (int)ceil((visScr.bottom - o.y) / m_zoom));
            vis.IntersectRect(vis, CRect(0, 0, m_img.Width(), m_img.Height()));
            if (!vis.IsRectEmpty())
            {
                CBitmap32 comp;
                Compose(comp, vis);
                if (!comp.IsNull())
                {
                    CBitmapDC cdc(comp);
                    CRect dst = ImgRectToScreen(vis);
                    mem.SetStretchBltMode(m_zoom < 1.0 ? HALFTONE : COLORONCOLOR);
                    ::SetBrushOrgEx(mem.GetSafeHdc(), 0, 0, nullptr);
                    mem.StretchBlt(dst.left, dst.top, dst.Width(), dst.Height(),
                                   &cdc.DC(), 0, 0, vis.Width(), vis.Height(), SRCCOPY);

                    // 눈금선
                    if (m_grid)
                    {
                        int step = m_zoom >= 4.0 ? 1 : (m_zoom >= 1.0 ? 10 : 50);
                        COLORREF gc = RGB(200, 205, 212);
                        int x0 = ((vis.left + step - 1) / step) * step;
                        for (int x = x0; x <= vis.right; x += step)
                        {
                            int sx = o.x + Z(x);
                            mem.FillSolidRect(sx, dst.top, 1, dst.Height(), gc);
                        }
                        int y0 = ((vis.top + step - 1) / step) * step;
                        for (int y = y0; y <= vis.bottom; y += step)
                        {
                            int sy = o.y + Z(y);
                            mem.FillSolidRect(dst.left, sy, dst.Width(), 1, gc);
                        }
                    }
                }
            }
        }
        DrawOverlays(mem);
    }
    if (m_rulers)
        DrawRulers(mem);

    pdc.BitBlt(0, 0, cl.Width(), cl.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(oldBmp);
}

// 눈금 간격(이미지 픽셀): 큰 눈금 사이가 화면에서 50px 이상이 되도록 고른다
static int RulerStep(double zoom)
{
    static const int steps[] = { 1, 2, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, 10000 };
    for (int s : steps)
        if (s * zoom >= 50.0)
            return s;
    return 10000;
}

void CCanvasWnd::InvalidateRulers()
{
    if (!m_rulers)
        return;
    CRect cl;
    GetClientRect(&cl);
    InvalidateRect(CRect(0, 0, cl.right, m_rulerSize), FALSE);
    InvalidateRect(CRect(0, 0, m_rulerSize, cl.bottom), FALSE);
}

void CCanvasWnd::DrawRulers(CDC& dc)
{
    const ThemeColors& th = Theme();
    CRect cl;
    GetClientRect(&cl);
    int R = m_rulerSize;
    CRect top(R, 0, cl.right, R), left(0, R, R, cl.bottom);
    dc.FillSolidRect(top, th.rulerBg);
    dc.FillSolidRect(left, th.rulerBg);
    dc.FillSolidRect(0, 0, R, R, th.rulerBg);
    dc.FillSolidRect(0, R - 1, cl.right, 1, th.panelLine);
    dc.FillSolidRect(R - 1, 0, 1, cl.bottom, th.panelLine);

    CPoint o = Origin();
    int major = RulerStep(m_zoom);
    int minor = major >= 10 ? major / 10 : (major >= 5 ? 1 : major);
    if (minor * m_zoom < 4) minor = major / 5 > 0 ? major / 5 : major;
    if (minor <= 0) minor = 1;

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(th.rulerText);

    // 가로 눈금자
    {
        CRgn clip;
        clip.CreateRectRgnIndirect(&top);
        dc.SelectClipRgn(&clip);
        CFont* of = dc.SelectObject(&m_rulerFontH);
        int x0 = (int)floor((R - o.x) / m_zoom);
        int x1 = (int)ceil((cl.right - o.x) / m_zoom);
        int start = (int)floor((double)x0 / minor) * minor;
        for (int x = start; x <= x1; x += minor)
        {
            int sx = o.x + Z(x);
            bool isMajor = (x % major) == 0;
            int len = isMajor ? R * 3 / 5 : ((x % (minor * 5)) == 0 ? R / 3 : R / 5);
            dc.FillSolidRect(sx, R - len, 1, len, th.rulerTick);
            if (isMajor)
            {
                CString s;
                s.Format(_T("%d"), x);
                dc.TextOut(sx + 3, 1, s);
            }
        }
        if (m_hoverValid)
            dc.FillSolidRect(o.x + Z(m_hoverImg.x) + Z(1) / 2, 0, 1, R, th.accent);
        dc.SelectObject(of);
        dc.SelectClipRgn(nullptr);
    }

    // 세로 눈금자 (숫자는 90도 회전)
    {
        CRgn clip;
        clip.CreateRectRgnIndirect(&left);
        dc.SelectClipRgn(&clip);
        CFont* of = dc.SelectObject(&m_rulerFontV);
        int y0 = (int)floor((R - o.y) / m_zoom);
        int y1 = (int)ceil((cl.bottom - o.y) / m_zoom);
        int start = (int)floor((double)y0 / minor) * minor;
        for (int y = start; y <= y1; y += minor)
        {
            int sy = o.y + Z(y);
            bool isMajor = (y % major) == 0;
            int len = isMajor ? R * 3 / 5 : ((y % (minor * 5)) == 0 ? R / 3 : R / 5);
            dc.FillSolidRect(R - len, sy, len, 1, th.rulerTick);
            if (isMajor)
            {
                CString s;
                s.Format(_T("%d"), y);
                dc.TextOut(1, sy - 3, s);
            }
        }
        if (m_hoverValid)
            dc.FillSolidRect(0, o.y + Z(m_hoverImg.y) + Z(1) / 2, R, 1, th.accent);
        dc.SelectObject(of);
        dc.SelectClipRgn(nullptr);
    }
}

static void DrawDashedRect(CDC& dc, const CRect& r)
{
    CPen pen(PS_DOT, 1, RGB(0, 0, 0));
    CPen* op = dc.SelectObject(&pen);
    CGdiObject* ob = dc.SelectStockObject(NULL_BRUSH);
    int oldMode = dc.SetBkMode(OPAQUE);
    COLORREF oldBk = dc.SetBkColor(RGB(255, 255, 255));
    dc.Rectangle(r);
    dc.SetBkColor(oldBk);
    dc.SetBkMode(oldMode);
    dc.SelectObject(ob);
    dc.SelectObject(op);
}

static void DrawHandle(CDC& dc, CPoint c)
{
    CRect r(c.x - 4, c.y - 4, c.x + 5, c.y + 5);
    dc.FillSolidRect(r, RGB(255, 255, 255));
    CBrush br(RGB(0, 95, 184));
    dc.FrameRect(r, &br);
}

void CCanvasWnd::GetHandleCenters(const CRect& s, CPoint pts[8]) const
{
    int mx = (s.left + s.right) / 2, my = (s.top + s.bottom) / 2;
    pts[0] = CPoint(s.left, s.top);
    pts[1] = CPoint(mx, s.top);
    pts[2] = CPoint(s.right, s.top);
    pts[3] = CPoint(s.right, my);
    pts[4] = CPoint(s.right, s.bottom);
    pts[5] = CPoint(mx, s.bottom);
    pts[6] = CPoint(s.left, s.bottom);
    pts[7] = CPoint(s.left, my);
}

int CCanvasWnd::HitHandle(CPoint pt, const CRect& s) const
{
    CPoint c[8];
    GetHandleCenters(s, c);
    for (int i = 0; i < 8; ++i)
        if (abs(pt.x - c[i].x) <= 5 && abs(pt.y - c[i].y) <= 5)
            return i;
    return -1;
}

void CCanvasWnd::GetCanvasHandleCenters(CPoint pts[3]) const
{
    CRect s = ImgRectToScreen(CRect(0, 0, m_img.Width(), m_img.Height()));
    pts[0] = CPoint(s.right + 4, (s.top + s.bottom) / 2);   // 오른쪽
    pts[1] = CPoint(s.right + 4, s.bottom + 4);              // 오른쪽 아래
    pts[2] = CPoint((s.left + s.right) / 2, s.bottom + 4);   // 아래
}

int CCanvasWnd::HitCanvasHandle(CPoint pt) const
{
    CPoint c[3];
    GetCanvasHandleCenters(c);
    for (int i = 0; i < 3; ++i)
        if (abs(pt.x - c[i].x) <= 5 && abs(pt.y - c[i].y) <= 5)
            return i;
    return -1;
}

CRect CCanvasWnd::ShapeScreenRect() const
{
    CRect r(min(m_sp1.x, m_sp2.x), min(m_sp1.y, m_sp2.y), max(m_sp1.x, m_sp2.x) + 1, max(m_sp1.y, m_sp2.y) + 1);
    return ImgRectToScreen(r);
}

int CCanvasWnd::HitShapeHandle(CPoint pt) const
{
    if (m_pendingShape == SH_LINE)
    {
        int half = Z(1) / 2;
        CPoint a = ImgToScreen(m_sp1.x, m_sp1.y) + CPoint(half, half);
        CPoint b = ImgToScreen(m_sp2.x, m_sp2.y) + CPoint(half, half);
        if (abs(pt.x - a.x) <= 5 && abs(pt.y - a.y) <= 5) return 0;
        if (abs(pt.x - b.x) <= 5 && abs(pt.y - b.y) <= 5) return 1;
        return -1;
    }
    return HitHandle(pt, ShapeScreenRect());
}

void CCanvasWnd::DrawOverlays(CDC& dc)
{
    // 선택 영역
    if (m_selActive || m_drag == DRAG_SEL_NEW)
    {
        CRect s = ImgRectToScreen(m_selRect);
        DrawDashedRect(dc, s);
        if (m_selIsFree && !m_lifted && m_freePoly.size() > 2)
        {
            std::vector<POINT> pts;
            for (auto& p : m_freePoly)
                pts.push_back(ImgToScreen(p.x, p.y));
            pts.push_back(pts.front());
            CPen pen(PS_DOT, 1, RGB(0, 0, 0));
            CPen* op = dc.SelectObject(&pen);
            dc.SetBkColor(RGB(255, 255, 255));
            dc.Polyline(pts.data(), (int)pts.size());
            dc.SelectObject(op);
        }
        if (m_selActive && m_drag != DRAG_SEL_NEW)
        {
            CPoint c[8];
            GetHandleCenters(s, c);
            for (auto& p : c)
                DrawHandle(dc, p);
        }
    }

    // 자유 형식 선택 진행 중
    if (m_drag == DRAG_SEL_FREE && m_freePoly.size() > 1)
    {
        std::vector<POINT> pts;
        for (auto& p : m_freePoly)
            pts.push_back(ImgToScreen(p.x, p.y));
        CPen pen(PS_DOT, 1, RGB(0, 0, 0));
        CPen* op = dc.SelectObject(&pen);
        dc.SetBkColor(RGB(255, 255, 255));
        dc.Polyline(pts.data(), (int)pts.size());
        dc.SelectObject(op);
    }

    // 확정 전 도형
    if (m_shapePending && m_drag != DRAG_SHAPE_NEW)
    {
        if (m_pendingShape == SH_LINE)
        {
            int half = Z(1) / 2;
            DrawHandle(dc, ImgToScreen(m_sp1.x, m_sp1.y) + CPoint(half, half));
            DrawHandle(dc, ImgToScreen(m_sp2.x, m_sp2.y) + CPoint(half, half));
        }
        else
        {
            CRect s = ShapeScreenRect();
            DrawDashedRect(dc, s);
            CPoint c[8];
            GetHandleCenters(s, c);
            for (auto& p : c)
                DrawHandle(dc, p);
        }
    }

    // 텍스트 상자
    if (m_drag == DRAG_TEXT_NEW || m_textActive)
    {
        CRect s = ImgRectToScreen(m_textRect);
        s.InflateRect(1, 1);
        DrawDashedRect(dc, s);
    }

    // 캔버스 크기 조정
    if (m_drag == DRAG_CANVAS)
    {
        CRect s = ImgRectToScreen(CRect(0, 0, m_canvasNewSize.cx, m_canvasNewSize.cy));
        DrawDashedRect(dc, s);
    }
    else if (!m_selActive && !IsShapeBusy() && !m_textActive && m_drag == DRAG_NONE)
    {
        CPoint c[3];
        GetCanvasHandleCenters(c);
        for (auto& p : c)
            DrawHandle(dc, p);
    }

    // 다각형: 시작점 표시 (여기를 클릭하면 닫힘)
    if (m_polyActive && m_polyPts.size() >= 2)
    {
        int half = Z(1) / 2;
        DrawHandle(dc, ImgToScreen(m_polyPts[0].x, m_polyPts[0].y) + CPoint(half, half));
    }

    // 곡선: 끝점 표시
    if (m_curveStage > 0 && m_drag != DRAG_CURVE)
    {
        int half = Z(1) / 2;
        DrawHandle(dc, ImgToScreen(m_cv[0].x, m_cv[0].y) + CPoint(half, half));
        DrawHandle(dc, ImgToScreen(m_cv[3].x, m_cv[3].y) + CPoint(half, half));
    }

    // 지우개 미리보기
    if (m_hoverIn && m_tool == TOOL_ERASER && (m_drag == DRAG_NONE || m_drag == DRAG_STROKE))
    {
        int w = GetToolWidth();
        CRect r(m_hoverImg.x - w / 2, m_hoverImg.y - w / 2, m_hoverImg.x - w / 2 + w, m_hoverImg.y - w / 2 + w);
        CRect s = ImgRectToScreen(r);
        CBrush br(RGB(90, 90, 90));
        dc.FrameRect(s, &br);
    }
}

// ---------------------------------------------------------------------------
// 스크롤 메시지

void CCanvasWnd::OnHScroll(UINT nSBCode, UINT, CScrollBar*)
{
    SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };
    GetScrollInfo(SB_HORZ, &si);
    int pos = m_scroll.x;
    switch (nSBCode)
    {
    case SB_LINELEFT:  pos -= 40; break;
    case SB_LINERIGHT: pos += 40; break;
    case SB_PAGELEFT:  pos -= (int)si.nPage; break;
    case SB_PAGERIGHT: pos += (int)si.nPage; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: pos = si.nTrackPos; break;
    case SB_LEFT:  pos = 0; break;
    case SB_RIGHT: pos = si.nMax; break;
    default: return;
    }
    ScrollTo(pos, m_scroll.y);
}

void CCanvasWnd::OnVScroll(UINT nSBCode, UINT, CScrollBar*)
{
    SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL };
    GetScrollInfo(SB_VERT, &si);
    int pos = m_scroll.y;
    switch (nSBCode)
    {
    case SB_LINEUP:   pos -= 40; break;
    case SB_LINEDOWN: pos += 40; break;
    case SB_PAGEUP:   pos -= (int)si.nPage; break;
    case SB_PAGEDOWN: pos += (int)si.nPage; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: pos = si.nTrackPos; break;
    case SB_TOP:    pos = 0; break;
    case SB_BOTTOM: pos = si.nMax; break;
    default: return;
    }
    ScrollTo(m_scroll.x, pos);
}

BOOL CCanvasWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (nFlags & MK_CONTROL)
    {
        CPoint c = pt;
        ScreenToClient(&c);
        ZoomStep(zDelta > 0 ? 1 : -1, c);
        return TRUE;
    }
    int d = -zDelta * 80 / WHEEL_DELTA;
    if (nFlags & MK_SHIFT)
        ScrollTo(m_scroll.x + d, m_scroll.y);
    else
        ScrollTo(m_scroll.x, m_scroll.y + d);
    return TRUE;
}
