// ToolPanel.cpp : 상단 도구 패널
#include "framework.h"
#include "resource.h"
#include "ToolPanel.h"
#include "CanvasWnd.h"
#include "Theme.h"

using namespace Gdiplus;

static const COLORREF kPalette[20] =
{
    RGB(0, 0, 0),       RGB(127, 127, 127), RGB(136, 0, 21),    RGB(237, 28, 36),   RGB(255, 127, 39),
    RGB(255, 242, 0),   RGB(34, 177, 76),   RGB(0, 162, 232),   RGB(63, 72, 204),   RGB(163, 73, 164),
    RGB(255, 255, 255), RGB(195, 195, 195), RGB(185, 122, 87),  RGB(255, 174, 201), RGB(255, 201, 14),
    RGB(239, 228, 176), RGB(181, 230, 29),  RGB(153, 217, 234), RGB(112, 146, 190), RGB(200, 191, 231),
};

static Color GC(COLORREF c, BYTE a = 255)
{
    return Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

// 굵기 슬라이더: 작은 값을 세밀하게 고를 수 있도록 제곱 비율로 매핑
static double SizeToPos(int s)
{
    return sqrt((double)(s - MIN_TOOL_SIZE) / (MAX_TOOL_SIZE - MIN_TOOL_SIZE));
}

static int PosToSize(double t)
{
    t = ClampV(t, 0.0, 1.0);
    return MIN_TOOL_SIZE + (int)lround(t * t * (MAX_TOOL_SIZE - MIN_TOOL_SIZE));
}

static void RoundRectPath(GraphicsPath& p, const RectF& r, float rad)
{
    float d = rad * 2;
    p.AddArc(r.X, r.Y, d, d, 180, 90);
    p.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
    p.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
    p.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
    p.CloseFigure();
}

BEGIN_MESSAGE_MAP(CToolPanel, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONUP()
    ON_WM_MOUSEWHEEL()
    ON_MESSAGE(WM_SIZEPARENT, &CToolPanel::OnSizeParent)
END_MESSAGE_MAP()

CToolPanel::CToolPanel()
{
}

BOOL CToolPanel::Create(CWnd* pParent, CCanvasWnd* pCanvas)
{
    m_canvas = pCanvas;
    HDC hdc = ::GetDC(nullptr);
    m_dpi = ::GetDeviceCaps(hdc, LOGPIXELSX);
    ::ReleaseDC(nullptr, hdc);

    CString cls = AfxRegisterWndClass(0, ::LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr);
    if (!CWnd::CreateEx(0, cls, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                        CRect(0, 0, 100, GetPanelHeight()), pParent, AFX_IDW_CONTROLBAR_FIRST + 40))
        return FALSE;
    m_tip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    m_tip.SetMaxTipWidth(400);
    m_tip.Activate(TRUE);
    Layout();
    return TRUE;
}

// 프레임의 RecalcLayout이 보내는 WM_SIZEPARENT 처리: 위쪽에 고정 높이로 배치
LRESULT CToolPanel::OnSizeParent(WPARAM, LPARAM lParam)
{
    AFX_SIZEPARENTPARAMS* p = (AFX_SIZEPARENTPARAMS*)lParam;
    int h = GetPanelHeight();
    CRect rc(p->rect.left, p->rect.top, p->rect.right, p->rect.top + h);
    p->rect.top += h;
    p->sizeTotal.cy += h;
    if (p->hDWP)
        p->hDWP = ::DeferWindowPos(p->hDWP, m_hWnd, nullptr, rc.left, rc.top, rc.Width(), rc.Height(),
                                   SWP_NOACTIVATE | SWP_NOZORDER);
    return 0;
}

void CToolPanel::Refresh()
{
    if (!GetSafeHwnd())
        return;
    bool tm = m_canvas && m_canvas->GetTool() == TOOL_TEXT;
    if (tm != m_textMode)
        Layout();
    Invalidate(FALSE);
}

void CToolPanel::Layout()
{
    m_items.clear();
    m_groups.clear();
    m_textMode = m_canvas && m_canvas->GetTool() == TOOL_TEXT;
    m_hover = m_pressed = -1;

    int x = 10;
    auto begin = [&](LPCTSTR name) { m_groups.push_back({ S(x), 0, CString(name) }); };
    auto end = [&](int right)
    {
        m_groups.back().right = S(right);
        x = right + 15;
    };
    auto add = [&](int l, int t, int w, int h, Kind k, UINT cmd, PanelIcon ic, LPCTSTR tip,
                   int menu = M_NONE, int index = 0, LPCTSTR text = nullptr)
    {
        Item it;
        it.rc = CRect(S(l), S(t), S(l + w), S(t + h));
        it.kind = k;
        it.cmd = cmd;
        it.icon = ic;
        it.tip = tip;
        it.menu = menu;
        it.index = index;
        if (text) it.text = text;
        m_items.push_back(it);
    };

    // 실행 취소
    begin(_T("기록"));
    add(x, 9, 32, 30, K_CMD, ID_EDIT_UNDO, PI_UNDO, _T("실행 취소 (Ctrl+Z)"));
    add(x, 42, 32, 30, K_CMD, ID_EDIT_REDO, PI_REDO, _T("다시 실행 (Ctrl+Y)"));
    end(x + 32);

    // 선택
    begin(_T("선택"));
    add(x, 8, 46, 46, K_SELECT_MAIN, 0, PI_SELECT_RECT, _T("선택"));
    add(x, 56, 46, 17, K_DROP, 0, PI_NONE, _T("선택 옵션"), M_SELECT);
    end(x + 46);

    // 이미지
    begin(_T("이미지"));
    add(x, 7, 32, 22, K_CMD, ID_IMAGE_CROP, PI_CROP, _T("자르기 (Ctrl+Shift+X)"));
    add(x, 30, 32, 22, K_CMD, ID_IMAGE_RESIZE, PI_RESIZE, _T("크기 조정 및 기울이기 (Ctrl+W)"));
    add(x, 53, 32, 22, K_DROP, 0, PI_ROTATE, _T("회전 및 대칭 이동"), M_ROTATE);
    end(x + 32);

    // 도구
    begin(_T("도구"));
    {
        struct T { UINT cmd; PanelIcon ic; LPCTSTR tip; };
        static const T tools[6] =
        {
            { ID_TOOL_PENCIL, PI_PENCIL, _T("연필") },
            { ID_TOOL_FILL, PI_FILL, _T("채우기") },
            { ID_TOOL_TEXT, PI_TEXT, _T("텍스트") },
            { ID_TOOL_ERASER, PI_ERASER, _T("지우개") },
            { ID_TOOL_PICKER, PI_PICKER, _T("색 선택") },
            { ID_TOOL_MAGNIFIER, PI_MAGNIFIER, _T("돋보기 (왼쪽 클릭: 확대, 오른쪽 클릭: 축소)") },
        };
        for (int i = 0; i < 6; ++i)
            add(x + (i % 3) * 34, 9 + (i / 3) * 33, 32, 31, K_CMD, tools[i].cmd, tools[i].ic, tools[i].tip);
    }
    end(x + 3 * 34 - 2);

    // 브러시
    begin(_T("브러시"));
    add(x, 8, 46, 46, K_BRUSH_MAIN, 0, PI_BRUSH, _T("브러시"));
    add(x, 56, 46, 17, K_DROP, 0, PI_NONE, _T("브러시 종류"), M_BRUSH);
    end(x + 46);

    // 도형
    begin(_T("도형"));
    for (int i = 0; i < SH_COUNT; ++i)
        add(x + (i % 8) * 24, 7 + (i / 8) * 23, 22, 22, K_SHAPE, ID_SHAPE_FIRST + i, PI_NONE, g_shapeNames[i], M_NONE, i);
    {
        int x2 = x + 8 * 24 + 6;
        add(x2, 9, 100, 29, K_DROP, 0, PI_OUTLINE, _T("도형 윤곽선"), M_OUTLINE, 0, _T("윤곽선"));
        add(x2, 42, 100, 29, K_DROP, 0, PI_FILLMODE, _T("도형 채우기"), M_FILL, 0, _T("채우기"));
        end(x2 + 100);
    }

    // 크기
    begin(_T("크기"));
    add(x, 8, 120, 65, K_SLIDER, 0, PI_SIZE, _T("굵기 (Ctrl+] / Ctrl+[)"));
    end(x + 120);

    // 색
    begin(_T("색"));
    add(x, 8, 42, 65, K_SLOT, 0, PI_NONE, _T("색 1 (전경색)"), M_NONE, 0, _T("색 1"));
    add(x + 44, 8, 42, 65, K_SLOT, 0, PI_NONE, _T("색 2 (배경색)"), M_NONE, 1, _T("색 2"));
    {
        int px = x + 92;
        for (int i = 0; i < 20; ++i)
        {
            CString tip;
            COLORREF c = kPalette[i];
            tip.Format(_T("RGB(%d, %d, %d)\n왼쪽 클릭: 선택한 색, 오른쪽 클릭: 색 2"), GetRValue(c), GetGValue(c), GetBValue(c));
            add(px + (i % 10) * 23, 8 + (i / 10) * 23, 21, 21, K_PALETTE, 0, PI_NONE, tip, M_NONE, i);
        }
        for (int i = 0; i < CUSTOM_COLOR_COUNT; ++i)
            add(px + i * 23, 54, 21, 21, K_CUSTOM, 0, PI_NONE, _T("사용자 지정 색"), M_NONE, i);
        int ex = px + 10 * 23 + 6;
        add(ex, 8, 50, 65, K_CMD, ID_COLOR_EDIT, PI_EDITCOLOR, _T("색 편집"), M_NONE, 0, _T("색 편집"));
        end(ex + 50);
    }

    // 텍스트 (텍스트 도구일 때만)
    if (m_textMode)
    {
        begin(_T("텍스트"));
        add(x, 9, 156, 29, K_CMD, ID_TEXT_FONT, PI_FONT, _T("글꼴 선택"));
        add(x, 42, 58, 29, K_DROP, 0, PI_NONE, _T("글꼴 크기"), M_FONTSIZE);
        add(x + 62, 42, 30, 29, K_CMD, ID_TEXT_BOLD, PI_BOLD, _T("굵게 (Ctrl+B)"));
        add(x + 94, 42, 30, 29, K_CMD, ID_TEXT_ITALIC, PI_ITALIC, _T("기울임꼴 (Ctrl+I)"));
        add(x + 126, 42, 30, 29, K_CMD, ID_TEXT_UNDERLINE, PI_UNDERLINE, _T("밑줄 (Ctrl+U)"));
        add(x + 162, 8, 50, 65, K_CMD, ID_TEXT_OPAQUE, PI_OPAQUE, _T("배경 채우기 (색 2)"), M_NONE, 0, _T("배경"));
        end(x + 212);
    }

    // 도구 설명
    if (m_tip.GetSafeHwnd())
    {
        for (int i = 1; i <= m_tipCount; ++i)
            m_tip.DelTool(this, i);
        for (int i = 0; i < (int)m_items.size(); ++i)
            m_tip.AddTool(this, m_items[i].tip, m_items[i].rc, i + 1);
        m_tipCount = (int)m_items.size();
    }
    Invalidate(FALSE);
}

int CToolPanel::HitTest(CPoint pt) const
{
    for (int i = 0; i < (int)m_items.size(); ++i)
        if (m_items[i].rc.PtInRect(pt))
            return i;
    return -1;
}

bool CToolPanel::IsChecked(const Item& it) const
{
    if (!m_canvas) return false;
    PaintTool t = m_canvas->GetTool();
    switch (it.kind)
    {
    case K_CMD:
        if (it.cmd >= ID_TOOL_SELECT_RECT && it.cmd <= ID_TOOL_BRUSH)
            return t == (PaintTool)(it.cmd - ID_TOOL_SELECT_RECT);
        if (it.cmd == ID_TEXT_BOLD) return m_canvas->GetTextFont().lfWeight >= FW_BOLD;
        if (it.cmd == ID_TEXT_ITALIC) return m_canvas->GetTextFont().lfItalic != 0;
        if (it.cmd == ID_TEXT_UNDERLINE) return m_canvas->GetTextFont().lfUnderline != 0;
        if (it.cmd == ID_TEXT_OPAQUE) return m_canvas->GetTextOpaque();
        return false;
    case K_SELECT_MAIN:
        return t == TOOL_SELECT_RECT || t == TOOL_SELECT_FREE;
    case K_BRUSH_MAIN:
        return t == TOOL_BRUSH;
    case K_SHAPE:
        return t == TOOL_SHAPE && m_canvas->GetShape() == (ShapeType)it.index;
    default:
        return false;
    }
}

bool CToolPanel::IsEnabled(const Item& it) const
{
    if (!m_canvas) return true;
    if (it.kind == K_SLIDER) return m_canvas->IsSizeEnabled();
    if (it.kind != K_CMD) return true;
    switch (it.cmd)
    {
    case ID_EDIT_UNDO:  return m_canvas->CanUndo() || m_canvas->IsTextActive() || m_canvas->HasSelection();
    case ID_EDIT_REDO:  return m_canvas->CanRedo();
    case ID_IMAGE_CROP: return m_canvas->HasSelection();
    default: return true;
    }
}

void CToolPanel::SendCommand(UINT cmd)
{
    CWnd* f = GetParent();
    if (f && cmd)
        f->SendMessage(WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
}

void CToolPanel::Activate(const Item& it, bool right)
{
    switch (it.kind)
    {
    case K_CMD:
        if (!right && IsEnabled(it))
            SendCommand(it.cmd);
        break;
    case K_SELECT_MAIN:
        if (!right)
            SendCommand(m_canvas->GetSelectMode() == TOOL_SELECT_FREE ? ID_TOOL_SELECT_FREE : ID_TOOL_SELECT_RECT);
        break;
    case K_BRUSH_MAIN:
        if (!right)
            SendCommand(ID_BRUSH_FIRST + m_canvas->GetBrush());
        break;
    case K_SHAPE:
        if (!right)
            SendCommand(it.cmd);
        break;
    case K_DROP:
        if (!right)
            ShowMenu(it);
        break;
    case K_SLOT:
        if (!right)
            m_canvas->SetActiveSlot(it.index);
        break;
    case K_PALETTE:
        m_canvas->OnPaletteClick(kPalette[it.index], right);
        break;
    case K_CUSTOM:
    {
        COLORREF c = m_canvas->GetCustomColor(it.index);
        if (c != CLR_INVALID)
            m_canvas->OnPaletteClick(c, right);
        else if (!right)
            SendCommand(ID_COLOR_EDIT);
        break;
    }
    }
    Invalidate(FALSE);
}

void CToolPanel::ShowMenu(const Item& it)
{
    CMenu m;
    m.CreatePopupMenu();
    auto chk = [](bool b) { return (UINT)(MF_STRING | (b ? MF_CHECKED : MF_UNCHECKED)); };
    PaintTool tool = m_canvas->GetTool();

    switch (it.menu)
    {
    case M_SELECT:
    {
        bool isSel = tool == TOOL_SELECT_RECT || tool == TOOL_SELECT_FREE;
        m.AppendMenu(chk(isSel && tool == TOOL_SELECT_RECT), ID_TOOL_SELECT_RECT, _T("사각형 선택"));
        m.AppendMenu(chk(isSel && tool == TOOL_SELECT_FREE), ID_TOOL_SELECT_FREE, _T("자유 형식 선택"));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING, ID_EDIT_SELECT_ALL, _T("모두 선택\tCtrl+A"));
        m.AppendMenu(MF_STRING | (m_canvas->HasSelection() ? 0 : MF_GRAYED), ID_EDIT_CLEAR, _T("선택 영역 삭제\tDel"));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING, ID_SELECT_TRANSPARENT, _T("투명 선택"));
        break;
    }
    case M_ROTATE:
        m.AppendMenu(MF_STRING, ID_IMAGE_ROTATE_RIGHT, _T("오른쪽으로 90도 회전"));
        m.AppendMenu(MF_STRING, ID_IMAGE_ROTATE_LEFT, _T("왼쪽으로 90도 회전"));
        m.AppendMenu(MF_STRING, ID_IMAGE_ROTATE_180, _T("180도 회전"));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING, ID_IMAGE_FLIP_V, _T("세로로 대칭 이동"));
        m.AppendMenu(MF_STRING, ID_IMAGE_FLIP_H, _T("가로로 대칭 이동"));
        m.AppendMenu(MF_SEPARATOR);
        m.AppendMenu(MF_STRING, ID_IMAGE_INVERT, _T("색 반전\tCtrl+Shift+I"));
        break;
    case M_BRUSH:
        for (int i = 0; i < BRUSH_COUNT; ++i)
            m.AppendMenu(chk(tool == TOOL_BRUSH && m_canvas->GetBrush() == i), ID_BRUSH_FIRST + i, g_brushNames[i]);
        break;
    case M_OUTLINE:
        m.AppendMenu(chk(!m_canvas->GetOutline()), ID_OUTLINE_NONE, _T("윤곽선 없음"));
        m.AppendMenu(chk(m_canvas->GetOutline()), ID_OUTLINE_SOLID, _T("단색 (색 1)"));
        break;
    case M_FILL:
        m.AppendMenu(chk(!m_canvas->GetFillShape()), ID_FILL_NONE, _T("채우기 없음"));
        m.AppendMenu(chk(m_canvas->GetFillShape()), ID_FILL_SOLID, _T("단색 (색 2)"));
        break;
    case M_FONTSIZE:
        for (int i = 0; i < FONT_SIZE_COUNT; ++i)
        {
            CString s;
            s.Format(_T("%d"), g_fontSizes[i]);
            m.AppendMenu(chk(m_canvas->GetFontPt() == g_fontSizes[i]), ID_FONTSIZE_FIRST + i, s);
        }
        break;
    default:
        return;
    }

    // 투명 선택 체크 상태
    if (it.menu == M_SELECT)
    {
        CCmdUI ui;
        ui.m_nID = ID_SELECT_TRANSPARENT;
        ui.m_pMenu = &m;
        ui.m_nIndex = m.GetMenuItemCount() - 1;
        ui.m_nIndexMax = m.GetMenuItemCount();
        ui.DoUpdate(GetParentFrame(), FALSE);
    }

    CRect r = it.rc;
    ClientToScreen(&r);
    UINT cmd = m.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                r.left, r.bottom, this);
    if (cmd)
        SendCommand(cmd);
}

// ---------------------------------------------------------------------------
// 그리기

BOOL CToolPanel::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

void CToolPanel::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    Invalidate(FALSE);
}

void CToolPanel::DrawItem(Graphics& g, const Item& it, int idx)
{
    RectF rf((REAL)it.rc.left, (REAL)it.rc.top, (REAL)it.rc.Width(), (REAL)it.rc.Height());
    bool hot = idx == m_hover;
    bool pressed = hot && idx == m_pressed;
    bool checked = IsChecked(it);
    bool enabled = IsEnabled(it);
    float k = m_dpi / 96.0f;
    const ThemeColors& th = Theme();
    Color ink = GC(enabled ? th.text : th.disabled);

    // 배경
    if (it.kind != K_PALETTE && it.kind != K_CUSTOM && it.kind != K_SLIDER)
    {
        Color bg(0, 0, 0, 0);
        if (checked) bg = GC(th.checked);
        if (hot && enabled && (pressed || !checked)) bg = GC(pressed ? th.pressed : th.hover);
        if (bg.GetA())
        {
            GraphicsPath p;
            RoundRectPath(p, rf, 4 * k);
            SolidBrush b(bg);
            g.FillPath(&b, &p);
            if (checked)
            {
                Pen bp(GC(th.checkedBorder), hot ? 1.6f : 1.0f);
                g.DrawPath(&bp, &p);
            }
        }
    }

    switch (it.kind)
    {
    case K_SLIDER:
        DrawSlider(g, it, enabled);
        break;
    case K_CMD:
    case K_SELECT_MAIN:
    case K_BRUSH_MAIN:
    case K_DROP:
    {
        PanelIcon icon = it.icon;
        if (it.kind == K_SELECT_MAIN && m_canvas->GetSelectMode() == TOOL_SELECT_FREE)
            icon = PI_SELECT_FREE;

        CString text = it.text;
        if (it.cmd == ID_TEXT_FONT)
            text.Format(_T("%s"), m_canvas->GetTextFont().lfFaceName);
        if (it.menu == M_FONTSIZE)
            text.Format(_T("%dpt"), m_canvas->GetFontPt());

        FontFamily ff(L"Malgun Gothic");
        Font font(&ff, 12.0f * k, FontStyleRegular, UnitPixel);
        SolidBrush tb(ink);
        StringFormat sf;
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        sf.SetFormatFlags(StringFormatFlagsNoWrap);

        bool wide = rf.Width > rf.Height * 1.6f;
        if (icon == PI_NONE && text.IsEmpty())
        {
            DrawChevron(g, rf.X + rf.Width / 2, rf.Y + rf.Height / 2, k, ink);
        }
        else if (wide)
        {
            float x = rf.X + 6 * k;
            if (icon != PI_NONE)
            {
                float is = 18 * k;
                DrawPanelIcon(g, icon, RectF(x, rf.Y + (rf.Height - is) / 2, is, is), ink);
                x += is + 6 * k;
            }
            float right = rf.GetRight() - (it.kind == K_DROP ? 16 * k : 4 * k);
            g.DrawString(CStringW(text), -1, &font, RectF(x, rf.Y, right - x, rf.Height), &sf, &tb);
            if (it.kind == K_DROP)
                DrawChevron(g, rf.GetRight() - 9 * k, rf.Y + rf.Height / 2, k, ink);
        }
        else if (rf.Height >= 60 * k)
        {
            // 큰 버튼: 아이콘 + 아래쪽 텍스트 또는 화살표
            float is = 24 * k;
            DrawPanelIcon(g, icon, RectF(rf.X + (rf.Width - is) / 2, rf.Y + 9 * k, is, is), ink);
            if (!text.IsEmpty())
            {
                StringFormat cf;
                cf.SetAlignment(StringAlignmentCenter);
                cf.SetLineAlignment(StringAlignmentCenter);
                g.DrawString(CStringW(text), -1, &font, RectF(rf.X - 4 * k, rf.Y + 38 * k, rf.Width + 8 * k, 20 * k), &cf, &tb);
            }
            else if (it.kind == K_DROP)
                DrawChevron(g, rf.X + rf.Width / 2, rf.Y + 50 * k, k, ink);
        }
        else
        {
            float is = min(rf.Width, rf.Height) - 10 * k;
            is = min(is, 24 * k);
            float cx = rf.X + rf.Width / 2, cy = rf.Y + rf.Height / 2;
            if (it.kind == K_DROP && it.icon != PI_NONE)
                cx -= 3 * k;
            DrawPanelIcon(g, icon, RectF(cx - is / 2, cy - is / 2, is, is), ink);
            if (it.kind == K_DROP && it.icon != PI_NONE)
                DrawChevron(g, rf.GetRight() - 5 * k, cy, k * 0.6f, ink);
        }
        break;
    }
    case K_SHAPE:
    {
        RectF sr(rf.X + 4 * k, rf.Y + 4 * k, rf.Width - 8 * k, rf.Height - 8 * k);
        GraphicsPath p;
        AddShapePathF(p, (ShapeType)it.index, sr);
        Pen sp(ink, 1.2f * k);
        sp.SetLineJoin(LineJoinRound);
        g.DrawPath(&sp, &p);
        break;
    }
    case K_SLOT:
    {
        COLORREF c = m_canvas->GetColor(it.index);
        float d = 30 * k;
        RectF cr(rf.X + (rf.Width - d) / 2, rf.Y + 8 * k, d, d);
        if (m_canvas->GetActiveSlot() == it.index)
        {
            Pen ring(GC(th.accent), 2.0f * k);
            RectF rr = cr;
            rr.Inflate(3.5f * k, 3.5f * k);
            g.DrawEllipse(&ring, rr);
        }
        SolidBrush b(GC(c));
        g.FillEllipse(&b, cr);
        Pen border(GC(th.swatchBorder), 1.0f);
        g.DrawEllipse(&border, cr);

        FontFamily ff(L"Malgun Gothic");
        Font font(&ff, 12.0f * k, FontStyleRegular, UnitPixel);
        SolidBrush tb(ink);
        StringFormat cf;
        cf.SetAlignment(StringAlignmentCenter);
        cf.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(CStringW(it.text), -1, &font, RectF(rf.X - 4 * k, rf.Y + 42 * k, rf.Width + 8 * k, 20 * k), &cf, &tb);
        break;
    }
    case K_PALETTE:
    case K_CUSTOM:
    {
        COLORREF c = it.kind == K_PALETTE ? kPalette[it.index] : m_canvas->GetCustomColor(it.index);
        RectF cr = rf;
        cr.Inflate(-1.5f * k, -1.5f * k);
        if (c != CLR_INVALID)
        {
            SolidBrush b(GC(c));
            g.FillEllipse(&b, cr);
            Pen border(GC(th.swatchBorder), 1.0f);
            g.DrawEllipse(&border, cr);
        }
        else
        {
            SolidBrush b(GC(th.panelBg));
            g.FillEllipse(&b, cr);
            Pen border(GC(th.separator), 1.0f);
            border.SetDashStyle(DashStyleDot);
            g.DrawEllipse(&border, cr);
        }
        if (hot)
        {
            Pen ring(GC(th.text), 1.5f * k);
            RectF rr = rf;
            rr.Inflate(-0.5f, -0.5f);
            g.DrawEllipse(&ring, rr);
        }
        break;
    }
    }
}

void CToolPanel::OnPaint()
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
    mem.FillSolidRect(cl, th.panelBg);
    mem.FillSolidRect(0, cl.bottom - 1, cl.Width(), 1, th.panelLine);

    {
        Graphics g(mem.GetSafeHdc());
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.SetPixelOffsetMode(PixelOffsetModeHalf);
        float k = m_dpi / 96.0f;

        // 그룹 이름과 구분선
        FontFamily ff(L"Malgun Gothic");
        Font font(&ff, 11.5f * k, FontStyleRegular, UnitPixel);
        SolidBrush lb(GC(th.textDim));
        StringFormat cf;
        cf.SetAlignment(StringAlignmentCenter);
        cf.SetLineAlignment(StringAlignmentCenter);
        Pen sep(GC(th.separator), 1.0f);
        for (auto& gr : m_groups)
        {
            RectF lr((REAL)gr.left - 6 * k, 76 * k, (REAL)(gr.right - gr.left) + 12 * k, 18 * k);
            g.DrawString(CStringW(gr.name), -1, &font, lr, &cf, &lb);
            float sx = (REAL)gr.right + 7.5f * k;
            g.DrawLine(&sep, sx, 10 * k, sx, 88 * k);
        }

        for (int i = 0; i < (int)m_items.size(); ++i)
            DrawItem(g, m_items[i], i);
    }

    pdc.BitBlt(0, 0, cl.Width(), cl.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(ob);
}

// ---------------------------------------------------------------------------
// 마우스

BOOL CToolPanel::PreTranslateMessage(MSG* pMsg)
{
    if (m_tip.GetSafeHwnd() && pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST)
        m_tip.RelayEvent(pMsg);
    return CWnd::PreTranslateMessage(pMsg);
}

void CToolPanel::OnMouseMove(UINT, CPoint pt)
{
    if (!m_tracking)
    {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hWnd, 0 };
        ::TrackMouseEvent(&tme);
        m_tracking = true;
    }
    if (m_sliderDrag >= 0)
    {
        SetSizeFromX(m_items[m_sliderDrag], pt.x);
        return;
    }
    int h = HitTest(pt);
    if (h != m_hover)
    {
        m_hover = h;
        Invalidate(FALSE);
    }
}

CRect CToolPanel::SliderTrack(const Item& it) const
{
    return CRect(it.rc.left + S(10), it.rc.top + S(54), it.rc.right - S(10), it.rc.top + S(58));
}

void CToolPanel::SetSizeFromX(const Item& it, int x)
{
    CRect tr = SliderTrack(it);
    double t = (double)(x - tr.left) / max(1, (int)tr.Width());
    m_canvas->SetToolSize(PosToSize(t));
    Invalidate(FALSE);
}

void CToolPanel::DrawSlider(Graphics& g, const Item& it, bool enabled)
{
    const ThemeColors& th = Theme();
    float k = m_dpi / 96.0f;
    RectF rf((REAL)it.rc.left, (REAL)it.rc.top, (REAL)it.rc.Width(), (REAL)it.rc.Height());
    int size = m_canvas->GetToolSize();
    Color ink = GC(enabled ? th.text : th.disabled);

    // 굵기 미리보기 (최대 24px 로 표시)
    float pw = min((float)size, 24.0f) * k;
    pw = max(pw, 1.0f);
    Pen preview(ink, pw);
    preview.SetStartCap(LineCapRound);
    preview.SetEndCap(LineCapRound);
    float cy = rf.Y + 20 * k;
    g.DrawLine(&preview, rf.X + 22 * k, cy, rf.GetRight() - 22 * k, cy);

    // 값
    CStringW text;
    if (enabled)
        text.Format(L"%dpx", size);
    else
        text = L"—";
    FontFamily ff(L"Malgun Gothic");
    Font font(&ff, 12.0f * k, FontStyleRegular, UnitPixel);
    SolidBrush tb(ink);
    StringFormat cf;
    cf.SetAlignment(StringAlignmentCenter);
    cf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(text, -1, &font, RectF(rf.X, rf.Y + 34 * k, rf.Width, 16 * k), &cf, &tb);

    // 트랙과 손잡이
    CRect tr = SliderTrack(it);
    float t = (float)SizeToPos(size);
    float tx = tr.left + t * tr.Width();
    float ty = (tr.top + tr.bottom) / 2.0f;
    Pen track(GC(th.separator), 3.0f * k);
    track.SetStartCap(LineCapRound);
    track.SetEndCap(LineCapRound);
    g.DrawLine(&track, (REAL)tr.left, ty, (REAL)tr.right, ty);
    if (enabled)
    {
        Pen fill(GC(th.accent), 3.0f * k);
        fill.SetStartCap(LineCapRound);
        fill.SetEndCap(LineCapRound);
        g.DrawLine(&fill, (REAL)tr.left, ty, tx, ty);
    }
    float r = 7.0f * k;
    SolidBrush thumbBg(GC(th.panelBg));
    g.FillEllipse(&thumbBg, tx - r, ty - r, 2 * r, 2 * r);
    Pen thumb(GC(enabled ? th.accent : th.disabled), 2.0f * k);
    g.DrawEllipse(&thumb, tx - r, ty - r, 2 * r, 2 * r);
    SolidBrush dot(GC(enabled ? th.accent : th.disabled));
    float r2 = (m_sliderDrag >= 0 ? 4.5f : 3.5f) * k;
    g.FillEllipse(&dot, tx - r2, ty - r2, 2 * r2, 2 * r2);
}

BOOL CToolPanel::OnMouseWheel(UINT, short zDelta, CPoint pt)
{
    CPoint c = pt;
    ScreenToClient(&c);
    int h = HitTest(c);
    if (h >= 0 && m_items[h].kind == K_SLIDER && m_canvas->IsSizeEnabled())
    {
        m_canvas->SetToolSize(m_canvas->GetToolSize() + (zDelta > 0 ? 1 : -1));
        Invalidate(FALSE);
        return TRUE;
    }
    return FALSE;
}

void CToolPanel::OnMouseLeave()
{
    m_tracking = false;
    if (m_hover != -1)
    {
        m_hover = -1;
        Invalidate(FALSE);
    }
}

void CToolPanel::OnLButtonDown(UINT, CPoint pt)
{
    m_pressed = HitTest(pt);
    if (m_pressed >= 0 && m_items[m_pressed].kind == K_SLIDER)
    {
        int idx = m_pressed;
        m_pressed = -1;
        if (!IsEnabled(m_items[idx]))
            return;
        m_sliderDrag = idx;
        SetCapture();
        SetSizeFromX(m_items[idx], pt.x);
        return;
    }
    if (m_pressed >= 0)
        SetCapture();
    Invalidate(FALSE);
}

void CToolPanel::OnLButtonUp(UINT, CPoint pt)
{
    if (m_sliderDrag >= 0)
    {
        m_sliderDrag = -1;
        if (GetCapture() == this)
            ReleaseCapture();
        Invalidate(FALSE);
        return;
    }
    int p = m_pressed;
    m_pressed = -1;
    if (GetCapture() == this)
        ReleaseCapture();
    int h = HitTest(pt);
    if (p >= 0 && p == h)
    {
        Item it = m_items[p];   // Activate 중 Layout이 다시 실행될 수 있으므로 복사
        Activate(it, false);
    }
    Invalidate(FALSE);
}

void CToolPanel::OnRButtonUp(UINT, CPoint pt)
{
    int h = HitTest(pt);
    if (h >= 0)
    {
        Item it = m_items[h];
        Activate(it, true);
    }
}
