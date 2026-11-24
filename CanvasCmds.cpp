// CanvasCmds.cpp : 메뉴/패널 명령 처리
#include "framework.h"
#include "resource.h"
#include "CanvasWnd.h"
#include "MainFrm.h"
#include "Dialogs.h"

// ---------------------------------------------------------------------------
// 도구/색

void CCanvasWnd::SetTool(PaintTool t)
{
    if (t == m_tool)
    {
        NotifyState();
        return;
    }
    CommitText();
    CommitShape();
    CommitSelection();
    if (t == TOOL_PICKER)
        m_prevTool = m_tool;
    if (t == TOOL_SELECT_RECT || t == TOOL_SELECT_FREE)
        m_selectMode = t;
    m_tool = t;
    Invalidate(FALSE);
    UpdateStatus();
    NotifyState();
}

void CCanvasWnd::SetActiveSlot(int slot)
{
    m_activeSlot = ClampV(slot, 0, 1);
    NotifyState();
}

void CCanvasWnd::SetColor(int slot, COLORREF c)
{
    m_color[ClampV(slot, 0, 1)] = c;
    if (m_edit.GetSafeHwnd() && m_textActive)
        m_edit.Invalidate();
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::OnPaletteClick(COLORREF c, bool right)
{
    SetColor(right ? 1 : m_activeSlot, c);
}

void CCanvasWnd::AddCustomColor(COLORREF c)
{
    for (int i = 0; i < CUSTOM_COLOR_COUNT; ++i)
        if (m_custom[i] == c)
            return;
    for (int i = CUSTOM_COLOR_COUNT - 1; i > 0; --i)
        m_custom[i] = m_custom[i - 1];
    m_custom[0] = c;
}

void CCanvasWnd::OnToolCmd(UINT id)
{
    SetTool((PaintTool)(id - ID_TOOL_SELECT_RECT));
}

void CCanvasWnd::OnUpdateToolCmd(CCmdUI* pCmdUI)
{
    pCmdUI->SetRadio(m_tool == (PaintTool)(pCmdUI->m_nID - ID_TOOL_SELECT_RECT));
}

void CCanvasWnd::OnShapeCmd(UINT id)
{
    CommitShape();
    m_shape = (ShapeType)(id - ID_SHAPE_FIRST);
    SetTool(TOOL_SHAPE);
    NotifyState();
}

void CCanvasWnd::OnUpdateShapeCmd(CCmdUI* pCmdUI)
{
    pCmdUI->SetRadio(m_tool == TOOL_SHAPE && m_shape == (ShapeType)(pCmdUI->m_nID - ID_SHAPE_FIRST));
}

void CCanvasWnd::OnBrushCmd(UINT id)
{
    m_brush = (BrushKind)(id - ID_BRUSH_FIRST);
    SetTool(TOOL_BRUSH);
    NotifyState();
}

void CCanvasWnd::OnUpdateBrushCmd(CCmdUI* pCmdUI)
{
    pCmdUI->SetRadio(m_tool == TOOL_BRUSH && m_brush == (BrushKind)(pCmdUI->m_nID - ID_BRUSH_FIRST));
}

void CCanvasWnd::OnSizeCmd(UINT id)
{
    int s = GetToolSize();
    // 작을 때는 1씩, 클수록 크게 변경
    int step = s < 10 ? 1 : (s < 30 ? 2 : 5);
    SetToolSize(id == ID_SIZE_INC ? s + step : s - step);
}

void CCanvasWnd::OnUpdateSizeCmd(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsSizeEnabled());
}

void CCanvasWnd::OnShapeModeCmd(UINT id)
{
    switch (id)
    {
    case ID_OUTLINE_NONE:  m_outline = false; if (!m_fillShape) m_fillShape = true; break;
    case ID_OUTLINE_SOLID: m_outline = true; break;
    case ID_FILL_NONE:     m_fillShape = false; if (!m_outline) m_outline = true; break;
    case ID_FILL_SOLID:    m_fillShape = true; break;
    }
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::OnUpdateShapeModeCmd(CCmdUI* pCmdUI)
{
    switch (pCmdUI->m_nID)
    {
    case ID_OUTLINE_NONE:  pCmdUI->SetRadio(!m_outline); break;
    case ID_OUTLINE_SOLID: pCmdUI->SetRadio(m_outline); break;
    case ID_FILL_NONE:     pCmdUI->SetRadio(!m_fillShape); break;
    case ID_FILL_SOLID:    pCmdUI->SetRadio(m_fillShape); break;
    }
}

void CCanvasWnd::OnColorEdit()
{
    static COLORREF s_cust[16] = {};
    CColorDialog dlg(m_color[m_activeSlot], CC_FULLOPEN | CC_RGBINIT, Frame());
    dlg.m_cc.lpCustColors = s_cust;
    if (dlg.DoModal() != IDOK)
        return;
    COLORREF c = dlg.GetColor();
    AddCustomColor(c);
    SetColor(m_activeSlot, c);
}

void CCanvasWnd::OnColorSwap()
{
    COLORREF t = m_color[0];
    m_color[0] = m_color[1];
    m_color[1] = t;
    Invalidate(FALSE);
    NotifyState();
}

// ---------------------------------------------------------------------------
// 편집

void CCanvasWnd::OnEditUndo()
{
    if (IsTextEditFocused())
    {
        if (m_edit.CanUndo())
            m_edit.Undo();
        else
            CancelText();
        return;
    }
    Undo();
}

void CCanvasWnd::OnEditRedo()
{
    if (IsTextEditFocused())
        return;
    Redo();
}

void CCanvasWnd::OnEditCut()
{
    if (IsTextEditFocused()) { m_edit.Cut(); return; }
    if (!m_selActive) return;
    CopySelection();
    DeleteSelection();
}

void CCanvasWnd::OnEditCopy()
{
    if (IsTextEditFocused()) { m_edit.Copy(); return; }
    CopySelection();
}

void CCanvasWnd::OnEditPaste()
{
    if (IsTextEditFocused()) { m_edit.Paste(); return; }
    PasteImage();
}

void CCanvasWnd::OnEditSelectAll()
{
    if (IsTextEditFocused()) { m_edit.SetSel(0, -1); return; }
    SelectAll();
}

void CCanvasWnd::OnEditClear()
{
    if (IsTextEditFocused()) { m_edit.Clear(); return; }
    DeleteSelection();
}

void CCanvasWnd::OnUpdateEditUndo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CanUndo() || m_textActive || m_selActive);
}

void CCanvasWnd::OnUpdateEditRedo(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(CanRedo());
}

void CCanvasWnd::OnUpdateEditCutCopy(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_selActive || IsTextEditFocused());
}

void CCanvasWnd::OnUpdateEditPaste(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(IsTextEditFocused() || ::IsClipboardFormatAvailable(CF_BITMAP) ||
                   ::IsClipboardFormatAvailable(CF_DIB));
}

void CCanvasWnd::OnUpdateEditClear(CCmdUI* pCmdUI)
{
    pCmdUI->Enable(m_selActive || IsShapeBusy() || IsTextEditFocused());
}

void CCanvasWnd::CopySelection()
{
    CBitmap32 b;
    if (!GetSelectionImage(b))
        return;
    ::GdiFlush();
    for (int y = 0; y < b.Height(); ++y)
    {
        DWORD* r = b.Row(y);
        for (int x = 0; x < b.Width(); ++x)
            if ((r[x] >> 24) < 128)
                r[x] = 0xFFFFFFFF;
    }

    HDC scr = ::GetDC(nullptr);
    HBITMAP ddb = ::CreateCompatibleBitmap(scr, b.Width(), b.Height());
    HDC mem = ::CreateCompatibleDC(scr);
    HGDIOBJ old = ::SelectObject(mem, ddb);
    {
        CBitmapDC src(b);
        ::BitBlt(mem, 0, 0, b.Width(), b.Height(), src.GetSafeHdc(), 0, 0, SRCCOPY);
    }
    ::SelectObject(mem, old);
    ::DeleteDC(mem);
    ::ReleaseDC(nullptr, scr);

    if (OpenClipboard())
    {
        ::EmptyClipboard();
        if (!::SetClipboardData(CF_BITMAP, ddb))
            ::DeleteObject(ddb);
        ::CloseClipboard();
    }
    else
        ::DeleteObject(ddb);
}

void CCanvasWnd::PasteImage()
{
    if (!OpenClipboard())
        return;
    CBitmap32 b;
    HBITMAP hb = (HBITMAP)::GetClipboardData(CF_BITMAP);
    if (hb)
    {
        BITMAP bm = {};
        ::GetObject(hb, sizeof(bm), &bm);
        if (b.Create(bm.bmWidth, abs(bm.bmHeight), 0xFFFFFFFF))
        {
            CBitmapDC dst(b);
            HDC mem = ::CreateCompatibleDC(dst.GetSafeHdc());
            HGDIOBJ old = ::SelectObject(mem, hb);
            ::BitBlt(dst.GetSafeHdc(), 0, 0, b.Width(), b.Height(), mem, 0, 0, SRCCOPY);
            ::SelectObject(mem, old);
            ::DeleteDC(mem);
        }
    }
    ::CloseClipboard();
    if (b.IsNull())
        return;
    b.SetAlphaAll(255);

    CommitText();
    CommitShape();
    CommitSelection();
    PushUndo();
    if (b.Width() > m_img.Width() || b.Height() > m_img.Height())
        ResizeCanvas(max(m_img.Width(), b.Width()), max(m_img.Height(), b.Height()));

    CPoint tl = ScreenToImg(ViewRect().TopLeft());
    tl.x = ClampV((int)tl.x, 0, m_img.Width() - b.Width());
    tl.y = ClampV((int)tl.y, 0, m_img.Height() - b.Height());

    m_tool = TOOL_SELECT_RECT;
    m_selectMode = TOOL_SELECT_RECT;
    m_selRect = CRect(tl, b.Size());
    m_float = std::move(b);
    m_selActive = true;
    m_lifted = true;
    m_selIsFree = false;
    m_floatDirty = true;
    OnImageSizeChanged();
}

void CCanvasWnd::DeleteSelection()
{
    if (IsShapeBusy() && !m_selActive)
    {
        m_shapePending = false;
        m_curveStage = 0;
        m_polyActive = false;
        m_polyPts.clear();
        Invalidate(FALSE);
        NotifyState();
        return;
    }
    if (!m_selActive)
        return;
    Lift();
    ClearSelection();
    Invalidate(FALSE);
    UpdateStatus();
    NotifyState();
}

void CCanvasWnd::SelectAll()
{
    CommitText();
    CommitShape();
    CommitSelection();
    m_tool = TOOL_SELECT_RECT;
    m_selectMode = TOOL_SELECT_RECT;
    m_selRect = CRect(0, 0, m_img.Width(), m_img.Height());
    m_selActive = true;
    m_lifted = false;
    m_selIsFree = false;
    Invalidate(FALSE);
    UpdateStatus();
    NotifyState();
}

void CCanvasWnd::OnSelectTransparent()
{
    m_transparentSel = !m_transparentSel;
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::OnUpdateSelectTransparent(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_transparentSel);
}

// ---------------------------------------------------------------------------
// 이미지

void CCanvasWnd::CropToSelection()
{
    if (!m_selActive)
        return;
    CommitText();
    CommitShape();
    Lift();
    BakeFloat();
    CBitmap32 res;
    if (!res.Create(m_selRect.Width(), m_selRect.Height(), 0xFFFFFFFF))
        return;
    CompositeFloat(res, 0, 0);
    m_img = std::move(res);
    ClearSelection();
    OnImageSizeChanged();
}

void CCanvasWnd::ApplyTransform(std::function<CBitmap32(const CBitmap32&, bool)> fn)
{
    CommitText();
    CommitShape();
    if (m_selActive)
    {
        Lift();
        BakeFloat();
        CBitmap32 r = fn(m_float, true);
        if (r.IsNull())
            return;
        m_float = std::move(r);
        m_selRect = CRect(m_selRect.TopLeft(), m_float.Size());
        m_floatDirty = true;
    }
    else
    {
        CBitmap32 r = fn(m_img, false);
        if (r.IsNull())
            return;
        PushUndo();
        m_img = std::move(r);
    }
    OnImageSizeChanged();
}

void CCanvasWnd::OnImageCmd(UINT id)
{
    switch (id)
    {
    case ID_IMAGE_CROP:
        CropToSelection();
        break;
    case ID_IMAGE_ROTATE_RIGHT:
        ApplyTransform([](const CBitmap32& b, bool) { return b.Rotate90(true); });
        break;
    case ID_IMAGE_ROTATE_LEFT:
        ApplyTransform([](const CBitmap32& b, bool) { return b.Rotate90(false); });
        break;
    case ID_IMAGE_ROTATE_180:
        ApplyTransform([](const CBitmap32& b, bool) { return b.Rotate180(); });
        break;
    case ID_IMAGE_FLIP_V:
        ApplyTransform([](const CBitmap32& b, bool) { return b.FlipV(); });
        break;
    case ID_IMAGE_FLIP_H:
        ApplyTransform([](const CBitmap32& b, bool) { return b.FlipH(); });
        break;
    case ID_IMAGE_INVERT:
        ApplyTransform([](const CBitmap32& b, bool)
        {
            CBitmap32 r = b;
            r.InvertRGB();
            return r;
        });
        break;
    case ID_IMAGE_RESIZE:
    {
        CommitText();
        CommitShape();
        CSize cur = m_selActive ? m_selRect.Size() : m_img.Size();
        CResizeDlg dlg(cur.cx, cur.cy, Frame());
        if (dlg.DoModal() != IDOK)
            return;
        int nw = dlg.m_newW, nh = dlg.m_newH;
        double sh = dlg.m_skewH, sv = dlg.m_skewV;
        DWORD bg = ToPixel(m_color[1]);
        ApplyTransform([=](const CBitmap32& b, bool isFloat)
        {
            CBitmap32 r = (nw != b.Width() || nh != b.Height()) ? b.Scale(nw, nh) : b;
            if (fabs(sh) > 1e-9 || fabs(sv) > 1e-9)
                r = r.Skew(sh, sv, isFloat ? 0 : bg);
            return r;
        });
        break;
    }
    case ID_IMAGE_PROPERTIES:
    {
        FlattenPending();
        CCanvasSizeDlg dlg(m_img.Width(), m_img.Height(), Frame());
        if (dlg.DoModal() != IDOK)
            return;
        if (dlg.m_w == m_img.Width() && dlg.m_h == m_img.Height())
            return;
        PushUndo();
        ResizeCanvas(dlg.m_w, dlg.m_h);
        OnImageSizeChanged();
        break;
    }
    case ID_IMAGE_CLEAR:
        FlattenPending();
        PushUndo();
        m_img.Fill(ToPixel(m_color[1]));
        OnImageSizeChanged();
        break;
    }
}

void CCanvasWnd::OnUpdateImageCmd(CCmdUI* pCmdUI)
{
    if (pCmdUI->m_nID == ID_IMAGE_CROP)
        pCmdUI->Enable(m_selActive);
    else
        pCmdUI->Enable(TRUE);
}

// ---------------------------------------------------------------------------
// 보기

void CCanvasWnd::OnZoomCmd(UINT id)
{
    CRect cl = ViewRect();
    CPoint center = cl.CenterPoint();
    switch (id)
    {
    case ID_VIEW_ZOOM_IN:  ZoomStep(1, center); break;
    case ID_VIEW_ZOOM_OUT: ZoomStep(-1, center); break;
    case ID_VIEW_ZOOM_100: SetZoom(1.0, center); break;
    case ID_VIEW_ZOOM_FIT:
    {
        double z = min((double)(cl.Width() - 40) / m_img.Width(), (double)(cl.Height() - 40) / m_img.Height());
        SetZoom(ClampV(z, 0.05, 8.0), center);
        break;
    }
    }
}

void CCanvasWnd::OnViewGrid()
{
    m_grid = !m_grid;
    Invalidate(FALSE);
}

void CCanvasWnd::OnUpdateViewGrid(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_grid);
}

void CCanvasWnd::OnViewRulers()
{
    m_rulers = !m_rulers;
    AfxGetApp()->WriteProfileInt(_T("Settings"), _T("Rulers"), m_rulers ? 1 : 0);
    UpdateScrollBars();
    UpdateTextEditPos();
    Invalidate(FALSE);
}

void CCanvasWnd::OnUpdateViewRulers(CCmdUI* pCmdUI)
{
    pCmdUI->SetCheck(m_rulers);
}

// ---------------------------------------------------------------------------
// 텍스트 서식

void CCanvasWnd::OnTextCmd(UINT id)
{
    switch (id)
    {
    case ID_TEXT_FONT:
    {
        LOGFONT lf = m_lf;
        CClientDC dc(this);
        lf.lfHeight = -MulDiv(m_fontPt, dc.GetDeviceCaps(LOGPIXELSY), 72);
        CFontDialog dlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_EFFECTS, nullptr, Frame());
        dlg.m_cf.rgbColors = m_color[0];
        if (dlg.DoModal() != IDOK)
            break;
        dlg.GetCurrentFont(&lf);
        m_lf = lf;
        m_lf.lfQuality = ANTIALIASED_QUALITY;
        m_fontPt = max(1, dlg.GetSize() / 10);
        break;
    }
    case ID_TEXT_BOLD:
        m_lf.lfWeight = (m_lf.lfWeight >= FW_BOLD) ? FW_NORMAL : FW_BOLD;
        break;
    case ID_TEXT_ITALIC:
        m_lf.lfItalic = !m_lf.lfItalic;
        break;
    case ID_TEXT_UNDERLINE:
        m_lf.lfUnderline = !m_lf.lfUnderline;
        break;
    case ID_TEXT_OPAQUE:
        m_textOpaque = !m_textOpaque;
        if (m_edit.GetSafeHwnd())
            m_edit.Invalidate();
        break;
    }
    UpdateTextEditFont();
    if (m_textActive)
    {
        OnTextChange();
        m_edit.SetFocus();
    }
    Invalidate(FALSE);
    NotifyState();
}

void CCanvasWnd::OnUpdateTextCmd(CCmdUI* pCmdUI)
{
    switch (pCmdUI->m_nID)
    {
    case ID_TEXT_BOLD:      pCmdUI->SetCheck(m_lf.lfWeight >= FW_BOLD); break;
    case ID_TEXT_ITALIC:    pCmdUI->SetCheck(m_lf.lfItalic != 0); break;
    case ID_TEXT_UNDERLINE: pCmdUI->SetCheck(m_lf.lfUnderline != 0); break;
    case ID_TEXT_OPAQUE:    pCmdUI->SetCheck(m_textOpaque); break;
    default: pCmdUI->Enable(TRUE); break;
    }
}

void CCanvasWnd::OnFontSizeCmd(UINT id)
{
    m_fontPt = g_fontSizes[id - ID_FONTSIZE_FIRST];
    UpdateTextEditFont();
    if (m_textActive)
    {
        OnTextChange();
        m_edit.SetFocus();
    }
    NotifyState();
}

void CCanvasWnd::OnUpdateFontSizeCmd(CCmdUI* pCmdUI)
{
    pCmdUI->SetRadio(m_fontPt == g_fontSizes[pCmdUI->m_nID - ID_FONTSIZE_FIRST]);
}
