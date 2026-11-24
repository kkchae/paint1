#pragma once
#include "framework.h"

// 테마에 맞춰 직접 그리는 상태 표시줄 + 확대/축소 슬라이더
class CPaintStatusBar : public CStatusBar
{
public:
    static const int ZOOM_PANE = 5;
    static const double MIN_ZOOM;
    static const double MAX_ZOOM;

    void SetZoom(double z);
    std::function<void(double)> OnZoomChanged;   // 슬라이더를 끌었을 때
    std::function<void(int)> OnZoomStep;         // −/+ 단추

protected:
    double m_zoom = 1.0;
    bool m_dragging = false;

    static double ZoomToPos(double z);
    static double PosToZoom(double t);
    void GetSliderParts(CRect& minus, CRect& track, CRect& plus) const;
    void SetZoomFromX(int x);

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnNcPaint();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    DECLARE_MESSAGE_MAP()
};
