#pragma once
#include "framework.h"

class CCanvasWnd;

enum PanelIcon
{
    PI_NONE, PI_UNDO, PI_REDO, PI_SELECT_RECT, PI_SELECT_FREE, PI_CROP, PI_RESIZE, PI_ROTATE,
    PI_PENCIL, PI_FILL, PI_TEXT, PI_ERASER, PI_PICKER, PI_MAGNIFIER, PI_BRUSH, PI_SIZE,
    PI_OUTLINE, PI_FILLMODE, PI_EDITCOLOR, PI_BOLD, PI_ITALIC, PI_UNDERLINE, PI_OPAQUE, PI_FONT
};

void DrawPanelIcon(Gdiplus::Graphics& g, PanelIcon icon, const Gdiplus::RectF& r, const Gdiplus::Color& c);
void DrawChevron(Gdiplus::Graphics& g, float cx, float cy, float k, const Gdiplus::Color& c);

// Windows 11 그림판의 리본과 비슷한 상단 도구 패널
class CToolPanel : public CWnd
{
public:
    CToolPanel();
    BOOL Create(CWnd* pParent, CCanvasWnd* pCanvas);
    void Refresh();
    int GetPanelHeight() const { return S(98); }

protected:
    enum Kind { K_CMD, K_SELECT_MAIN, K_BRUSH_MAIN, K_SHAPE, K_DROP, K_SLOT, K_PALETTE, K_CUSTOM, K_SLIDER };
    enum Menu { M_NONE, M_SELECT, M_ROTATE, M_BRUSH, M_OUTLINE, M_FILL, M_FONTSIZE };

    struct Item
    {
        CRect rc;
        Kind kind;
        UINT cmd;
        int index;
        PanelIcon icon;
        int menu;
        CString text;
        CString tip;
    };
    struct Group
    {
        int left, right;
        CString name;
    };

    CCanvasWnd* m_canvas = nullptr;
    std::vector<Item> m_items;
    std::vector<Group> m_groups;
    CToolTipCtrl m_tip;
    int m_tipCount = 0;
    int m_hover = -1;
    int m_pressed = -1;
    int m_dpi = 96;
    bool m_textMode = false;
    bool m_tracking = false;
    int m_sliderDrag = -1;      // 끌고 있는 굵기 슬라이더 항목

    int S(int v) const { return MulDiv(v, m_dpi, 96); }
    float SF(float v) const { return v * m_dpi / 96.0f; }
    void Layout();
    CRect SliderTrack(const Item& it) const;
    void SetSizeFromX(const Item& it, int x);
    void DrawSlider(Gdiplus::Graphics& g, const Item& it, bool enabled);
    int HitTest(CPoint pt) const;
    bool IsChecked(const Item& it) const;
    bool IsEnabled(const Item& it) const;
    void Activate(const Item& it, bool right);
    void ShowMenu(const Item& it);
    void SendCommand(UINT cmd);
    void DrawItem(Gdiplus::Graphics& g, const Item& it, int idx);

    BOOL PreTranslateMessage(MSG* pMsg) override;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnMouseLeave();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint pt);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg LRESULT OnSizeParent(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()
};
