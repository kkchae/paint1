#pragma once
#include "framework.h"

// Windows 11 그림판의 도형 순서
enum ShapeType
{
    SH_LINE, SH_CURVE, SH_ELLIPSE, SH_RECT, SH_ROUNDRECT, SH_POLYGON,
    SH_TRIANGLE, SH_RTRIANGLE, SH_DIAMOND, SH_PENTAGON, SH_HEXAGON,
    SH_ARROW_R, SH_ARROW_L, SH_ARROW_U, SH_ARROW_D,
    SH_STAR4, SH_STAR5, SH_STAR6,
    SH_CALLOUT_RECT, SH_CALLOUT_OVAL, SH_CALLOUT_CLOUD,
    SH_HEART, SH_LIGHTNING,
    SH_COUNT
};

extern const TCHAR* g_shapeNames[SH_COUNT];

// p1, p2 (포함 좌표)를 대각선으로 하는 영역에 도형을 그린다. (곡선/다각형 제외)
void DrawShape(CDC* pDC, ShapeType t, CPoint p1, CPoint p2, int width,
               COLORREF outlineColor, COLORREF fillColor, bool outline, bool fill);

// 다각형 (closed=false 이면 열린 꺾은선)
void DrawPolyShape(CDC* pDC, const std::vector<POINT>& pts, bool closed, int width,
                   COLORREF outlineColor, COLORREF fillColor, bool outline, bool fill);

// 3차 베지어 곡선 (pts[0]=시작, pts[1..2]=조절점, pts[3]=끝)
void DrawBezierCurve(CDC* pDC, const POINT pts[4], int width, COLORREF color);

// UI 아이콘용 GDI+ 경로
void AddShapePathF(Gdiplus::GraphicsPath& path, ShapeType t, const Gdiplus::RectF& r);
