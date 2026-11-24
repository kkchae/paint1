#include "Shapes.h"

static const double PI = 3.14159265358979323846;

const TCHAR* g_shapeNames[SH_COUNT] =
{
    _T("선"), _T("곡선"), _T("타원"), _T("사각형"), _T("둥근 사각형"), _T("다각형"),
    _T("삼각형"), _T("직각 삼각형"), _T("마름모"), _T("오각형"), _T("육각형"),
    _T("오른쪽 화살표"), _T("왼쪽 화살표"), _T("위쪽 화살표"), _T("아래쪽 화살표"),
    _T("4각 별"), _T("5각 별"), _T("6각 별"),
    _T("둥근 사각형 설명선"), _T("타원형 설명선"), _T("구름 모양 설명선"),
    _T("하트"), _T("번개")
};

struct UPt { double x, y; };

// 하트: 베지어 곡선 1 + 6*3 점 (단위 좌표)
static const UPt kHeart[19] =
{
    {0.50, 1.00},
    {0.10, 0.70}, {0.00, 0.45}, {0.00, 0.28},
    {0.00, 0.10}, {0.15, 0.00}, {0.28, 0.00},
    {0.40, 0.00}, {0.50, 0.10}, {0.50, 0.20},
    {0.50, 0.10}, {0.60, 0.00}, {0.72, 0.00},
    {0.85, 0.00}, {1.00, 0.10}, {1.00, 0.28},
    {1.00, 0.45}, {0.90, 0.70}, {0.50, 1.00},
};

// 구름 설명선의 꼬리 (작은 원 2개: 중심 x, y, 반지름 x, 반지름 y)
static const double kCloudTail[2][4] =
{
    { 0.20, 0.87, 0.060, 0.050 },
    { 0.09, 0.96, 0.040, 0.035 },
};

static void NormalizeUnit(std::vector<UPt>& v)
{
    double minx = 1e9, miny = 1e9, maxx = -1e9, maxy = -1e9;
    for (auto& p : v)
    {
        minx = min(minx, p.x); maxx = max(maxx, p.x);
        miny = min(miny, p.y); maxy = max(maxy, p.y);
    }
    double w = maxx - minx, h = maxy - miny;
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;
    for (auto& p : v)
    {
        p.x = (p.x - minx) / w;
        p.y = (p.y - miny) / h;
    }
}

static void RegularPoly(std::vector<UPt>& v, int n, double startDeg)
{
    for (int k = 0; k < n; ++k)
    {
        double a = (startDeg + 360.0 * k / n) * PI / 180.0;
        v.push_back({ cos(a), sin(a) });
    }
    NormalizeUnit(v);
}

static void StarPoly(std::vector<UPt>& v, int n, double inner)
{
    for (int k = 0; k < 2 * n; ++k)
    {
        double r = (k % 2) ? inner : 1.0;
        double a = (-90.0 + 180.0 * k / n) * PI / 180.0;
        v.push_back({ r * cos(a), r * sin(a) });
    }
    NormalizeUnit(v);
}

// 둥근 사각형 설명선: 본체 (0,0)-(1,0.75), 꼬리 끝 (0.15,1)
static void CalloutRect(std::vector<UPt>& v)
{
    const double b = 0.75, rx = 0.10, ry = 0.13;
    auto corner = [&](double cx, double cy, double a0)
    {
        for (int i = 0; i <= 6; ++i)
        {
            double a = (a0 + 90.0 * i / 6) * PI / 180.0;
            v.push_back({ cx + rx * cos(a), cy + ry * sin(a) });
        }
    };
    corner(rx, ry, 180);            // 왼쪽 위
    corner(1 - rx, ry, 270);        // 오른쪽 위
    corner(1 - rx, b - ry, 0);      // 오른쪽 아래
    v.push_back({ 0.42, b });       // 꼬리
    v.push_back({ 0.15, 1.0 });
    v.push_back({ 0.26, b });
    corner(rx, b - ry, 90);         // 왼쪽 아래
}

// 타원형 설명선: 본체 타원 + 왼쪽 아래 꼬리
static void CalloutOval(std::vector<UPt>& v)
{
    const double cx = 0.5, cy = 0.39, rx = 0.5, ry = 0.39;
    const double a1 = 105.0, a2 = 128.0;   // 꼬리가 붙는 각도 범위 (화면 좌표, 시계 방향)
    const int N = 64;
    bool tailDone = false;
    for (int i = 0; i < N; ++i)
    {
        double deg = 360.0 * i / N;
        if (deg > a1 && deg < a2)
        {
            if (!tailDone)
            {
                double ra = a1 * PI / 180.0, rb = a2 * PI / 180.0;
                v.push_back({ cx + rx * cos(ra), cy + ry * sin(ra) });
                v.push_back({ 0.12, 1.0 });
                v.push_back({ cx + rx * cos(rb), cy + ry * sin(rb) });
                tailDone = true;
            }
            continue;
        }
        double a = deg * PI / 180.0;
        v.push_back({ cx + rx * cos(a), cy + ry * sin(a) });
    }
}

// 구름 본체: 가장자리가 볼록하게 반복되는 타원
static void CloudBody(std::vector<UPt>& v)
{
    const double cx = 0.5, cy = 0.40, rx = 0.44, ry = 0.33;
    const int bumps = 9, N = 180;
    for (int i = 0; i < N; ++i)
    {
        double t = 2 * PI * i / N;
        double k = 1.0 + 0.16 * fabs(sin(bumps * t / 2.0));
        v.push_back({ cx + rx * k * cos(t), cy + ry * k * sin(t) });
    }
}

// 다각형 도형의 단위 좌표. 다각형이 아니면 false.
static bool GetUnitPolygon(ShapeType t, std::vector<UPt>& v)
{
    v.clear();
    switch (t)
    {
    case SH_RECT:      v = { {0,0}, {1,0}, {1,1}, {0,1} }; return true;
    case SH_POLYGON:   v = { {0.1,0.25}, {0.6,0}, {1,0.45}, {0.7,1}, {0,0.8} }; return true;   // 아이콘용
    case SH_TRIANGLE:  v = { {0.5,0}, {1,1}, {0,1} }; return true;
    case SH_RTRIANGLE: v = { {0,0}, {1,1}, {0,1} }; return true;
    case SH_DIAMOND:   v = { {0.5,0}, {1,0.5}, {0.5,1}, {0,0.5} }; return true;
    case SH_PENTAGON:  RegularPoly(v, 5, -90); return true;
    case SH_HEXAGON:   RegularPoly(v, 6, 0); return true;
    case SH_ARROW_R:   v = { {0,0.25}, {0.6,0.25}, {0.6,0}, {1,0.5}, {0.6,1}, {0.6,0.75}, {0,0.75} }; return true;
    case SH_ARROW_L:   v = { {1,0.25}, {0.4,0.25}, {0.4,0}, {0,0.5}, {0.4,1}, {0.4,0.75}, {1,0.75} }; return true;
    case SH_ARROW_U:   v = { {0.25,1}, {0.25,0.4}, {0,0.4}, {0.5,0}, {1,0.4}, {0.75,0.4}, {0.75,1} }; return true;
    case SH_ARROW_D:   v = { {0.25,0}, {0.25,0.6}, {0,0.6}, {0.5,1}, {1,0.6}, {0.75,0.6}, {0.75,0} }; return true;
    case SH_STAR4:     StarPoly(v, 4, 0.4); return true;
    case SH_STAR5:     StarPoly(v, 5, 0.382); return true;
    case SH_STAR6:     StarPoly(v, 6, 0.55); return true;
    case SH_CALLOUT_RECT:  CalloutRect(v); return true;
    case SH_CALLOUT_OVAL:  CalloutOval(v); return true;
    case SH_CALLOUT_CLOUD: CloudBody(v); return true;
    case SH_LIGHTNING:
        v = { {0.40,0.00}, {0.72,0.30}, {0.58,0.38}, {0.90,0.66}, {0.76,0.72}, {1.00,1.00},
              {0.45,0.80}, {0.60,0.73}, {0.22,0.50}, {0.36,0.43}, {0.00,0.16} };
        NormalizeUnit(v);
        return true;
    default:
        return false;
    }
}

static POINT MapPt(const UPt& u, const CRect& r)
{
    POINT p;
    p.x = r.left + (LONG)lround(u.x * (r.right - r.left));
    p.y = r.top + (LONG)lround(u.y * (r.bottom - r.top));
    return p;
}

// 현재 경로(BeginPath ~ EndPath)에 도형 추가
static void AddShapePath(CDC* dc, ShapeType t, CPoint p1, CPoint p2)
{
    CRect r(min(p1.x, p2.x), min(p1.y, p2.y), max(p1.x, p2.x), max(p1.y, p2.y));
    switch (t)
    {
    case SH_LINE:
    case SH_CURVE:
        dc->MoveTo(p1);
        dc->LineTo(p2);
        return;
    case SH_ELLIPSE:
        dc->Ellipse(r.left, r.top, r.right + 1, r.bottom + 1);
        return;
    case SH_ROUNDRECT:
    {
        int rad = max(4, min(r.Width(), r.Height()) / 4);
        dc->RoundRect(r.left, r.top, r.right + 1, r.bottom + 1, rad, rad);
        return;
    }
    case SH_HEART:
    {
        POINT pts[19];
        for (int i = 0; i < 19; ++i)
            pts[i] = MapPt(kHeart[i], r);
        dc->MoveTo(pts[0]);
        dc->PolyBezierTo(pts + 1, 18);
        dc->CloseFigure();
        return;
    }
    default:
        break;
    }
    std::vector<UPt> u;
    if (!GetUnitPolygon(t, u))
        return;
    std::vector<POINT> pts;
    for (auto& q : u)
        pts.push_back(MapPt(q, r));
    dc->Polygon(pts.data(), (int)pts.size());

    if (t == SH_CALLOUT_CLOUD)
    {
        for (auto& c : kCloudTail)
        {
            POINT a = MapPt({ c[0] - c[2], c[1] - c[3] }, r);
            POINT b = MapPt({ c[0] + c[2], c[1] + c[3] }, r);
            dc->Ellipse(a.x, a.y, b.x + 1, b.y + 1);
        }
    }
}

// 펜/브러시를 만들어 경로를 그린다
template <class F>
static void StrokeFillPath(CDC* pDC, F addPath, bool roundJoin, int width,
                           COLORREF outC, COLORREF fillC, bool outline, bool fill)
{
    HDC hdc = pDC->GetSafeHdc();
    if (width < 1) width = 1;
    LOGBRUSH lb = { BS_SOLID, outC, 0 };
    DWORD style = PS_GEOMETRIC | PS_SOLID |
        (roundJoin ? (PS_ENDCAP_ROUND | PS_JOIN_ROUND) : (PS_ENDCAP_SQUARE | PS_JOIN_MITER));
    HPEN pen = ::ExtCreatePen(style, width, &lb, 0, nullptr);
    HBRUSH br = ::CreateSolidBrush(fillC);
    HGDIOBJ op = ::SelectObject(hdc, pen);
    HGDIOBJ ob = ::SelectObject(hdc, br);
    int oldFill = ::SetPolyFillMode(hdc, WINDING);
    FLOAT oldMiter = 10.0f;
    ::SetMiterLimit(hdc, 4.0f, &oldMiter);

    ::BeginPath(hdc);
    addPath();
    ::EndPath(hdc);
    if (outline && fill)
        ::StrokeAndFillPath(hdc);
    else if (fill)
        ::FillPath(hdc);
    else
        ::StrokePath(hdc);

    ::SetMiterLimit(hdc, oldMiter, nullptr);
    ::SetPolyFillMode(hdc, oldFill);
    ::SelectObject(hdc, op);
    ::SelectObject(hdc, ob);
    ::DeleteObject(pen);
    ::DeleteObject(br);
}

static void DrawThinLine(HDC hdc, const POINT* pts, int n, COLORREF c)
{
    HPEN pen = ::CreatePen(PS_SOLID, 1, c);
    HGDIOBJ op = ::SelectObject(hdc, pen);
    ::Polyline(hdc, pts, n);
    ::SetPixelV(hdc, pts[n - 1].x, pts[n - 1].y, c);
    ::SelectObject(hdc, op);
    ::DeleteObject(pen);
}

void DrawShape(CDC* pDC, ShapeType t, CPoint p1, CPoint p2, int width,
               COLORREF outlineColor, COLORREF fillColor, bool outline, bool fill)
{
    bool isLine = (t == SH_LINE || t == SH_CURVE);
    if (isLine)
    {
        outline = true;
        fill = false;
    }
    if (!outline && !fill)
        outline = true;

    if (isLine && width <= 1)
    {
        POINT pts[2] = { p1, p2 };
        DrawThinLine(pDC->GetSafeHdc(), pts, 2, outlineColor);
        return;
    }
    StrokeFillPath(pDC, [&]() { AddShapePath(pDC, t, p1, p2); }, isLine, width,
                   outlineColor, fillColor, outline, fill);
}

void DrawPolyShape(CDC* pDC, const std::vector<POINT>& pts, bool closed, int width,
                   COLORREF outlineColor, COLORREF fillColor, bool outline, bool fill)
{
    if (pts.empty())
        return;
    if (pts.size() < 3)
        closed = false;
    if (!closed)
    {
        if (width <= 1)
        {
            DrawThinLine(pDC->GetSafeHdc(), pts.data(), (int)pts.size(), outlineColor);
            return;
        }
        StrokeFillPath(pDC, [&]() { ::Polyline(pDC->GetSafeHdc(), pts.data(), (int)pts.size()); },
                       true, width, outlineColor, fillColor, true, false);
        return;
    }
    if (!outline && !fill)
        outline = true;
    StrokeFillPath(pDC, [&]() { ::Polygon(pDC->GetSafeHdc(), pts.data(), (int)pts.size()); },
                   false, width, outlineColor, fillColor, outline, fill);
}

void DrawBezierCurve(CDC* pDC, const POINT pts[4], int width, COLORREF color)
{
    HDC hdc = pDC->GetSafeHdc();
    if (width <= 1)
    {
        HPEN pen = ::CreatePen(PS_SOLID, 1, color);
        HGDIOBJ op = ::SelectObject(hdc, pen);
        ::PolyBezier(hdc, pts, 4);
        ::SetPixelV(hdc, pts[3].x, pts[3].y, color);
        ::SelectObject(hdc, op);
        ::DeleteObject(pen);
        return;
    }
    StrokeFillPath(pDC, [&]() { ::MoveToEx(hdc, pts[0].x, pts[0].y, nullptr); ::PolyBezierTo(hdc, pts + 1, 3); },
                   true, width, color, color, true, false);
}

void AddShapePathF(Gdiplus::GraphicsPath& path, ShapeType t, const Gdiplus::RectF& r)
{
    using namespace Gdiplus;
    auto M = [&](const UPt& u) { return PointF((REAL)(r.X + u.x * r.Width), (REAL)(r.Y + u.y * r.Height)); };
    switch (t)
    {
    case SH_LINE:
        path.AddLine(r.X, r.GetBottom(), r.GetRight(), r.Y);
        return;
    case SH_CURVE:
        path.AddBezier(M({ 0, 1 }), M({ 0.2, -0.2 }), M({ 0.8, 1.2 }), M({ 1, 0 }));
        return;
    case SH_ELLIPSE:
        path.AddEllipse(r);
        return;
    case SH_ROUNDRECT:
    {
        REAL d = min(r.Width, r.Height) / 2.5f;
        path.AddArc(r.X, r.Y, d, d, 180, 90);
        path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
        path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
        path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
        path.CloseFigure();
        return;
    }
    case SH_HEART:
    {
        PointF pts[19];
        for (int i = 0; i < 19; ++i)
            pts[i] = M(kHeart[i]);
        path.AddBeziers(pts, 19);
        path.CloseFigure();
        return;
    }
    default:
        break;
    }
    std::vector<UPt> u;
    if (!GetUnitPolygon(t, u))
        return;
    std::vector<PointF> pts;
    for (auto& q : u)
        pts.push_back(M(q));
    path.AddPolygon(pts.data(), (INT)pts.size());
    if (t == SH_CALLOUT_CLOUD)
    {
        for (auto& c : kCloudTail)
        {
            path.StartFigure();
            path.AddEllipse((REAL)(r.X + (c[0] - c[2]) * r.Width), (REAL)(r.Y + (c[1] - c[3]) * r.Height),
                            (REAL)(2 * c[2] * r.Width), (REAL)(2 * c[3] * r.Height));
        }
    }
}
