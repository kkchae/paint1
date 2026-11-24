// PanelIcons.cpp : 도구 패널 아이콘 (GDI+ 벡터 그림, 20x20 단위 좌표)
#include "framework.h"
#include "ToolPanel.h"

using namespace Gdiplus;

void DrawChevron(Graphics& g, float cx, float cy, float k, const Color& c)
{
    Pen pen(c, 1.3f * k);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    pen.SetLineJoin(LineJoinRound);
    PointF pts[3] = { PointF(cx - 3.5f * k, cy - 1.8f * k), PointF(cx, cy + 1.8f * k), PointF(cx + 3.5f * k, cy - 1.8f * k) };
    g.DrawLines(&pen, pts, 3);
}

static void DrawLetter(Graphics& g, const RectF& r, const wchar_t* s, INT style, const Color& c)
{
    FontFamily ff(L"Segoe UI");
    Font font(&ff, r.Height * 0.78f, style, UnitPixel);
    SolidBrush br(c);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    g.DrawString(s, -1, &font, r, &sf, &br);
}

void DrawPanelIcon(Graphics& g, PanelIcon icon, const RectF& r, const Color& c)
{
    float k = r.Width / 20.0f;
    auto P = [&](float x, float y) { return PointF(r.X + x * k, r.Y + y * k); };
    auto R = [&](float x, float y, float w, float h) { return RectF(r.X + x * k, r.Y + y * k, w * k, h * k); };

    Pen pen(c, max(1.0f, 1.35f * k));
    pen.SetLineJoin(LineJoinRound);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    SolidBrush br(c);

    switch (icon)
    {
    case PI_UNDO:
    case PI_REDO:
    {
        GraphicsState st = g.Save();
        if (icon == PI_REDO)
        {
            g.TranslateTransform(2 * r.X + r.Width, 0);
            g.ScaleTransform(-1, 1);
        }
        GraphicsPath path;
        path.AddLine(P(4, 8), P(12, 8));
        path.AddArc(R(8, 8, 8, 8), 270, 180);
        path.AddLine(P(12, 16), P(8, 16));
        g.DrawPath(&pen, &path);
        PointF head[3] = { P(7.5f, 4.5f), P(4, 8), P(7.5f, 11.5f) };
        g.DrawLines(&pen, head, 3);
        g.Restore(st);
        break;
    }
    case PI_SELECT_RECT:
    {
        pen.SetDashStyle(DashStyleDash);
        g.DrawRectangle(&pen, R(2.5f, 3.5f, 15, 13));
        break;
    }
    case PI_SELECT_FREE:
    {
        pen.SetDashStyle(DashStyleDash);
        PointF pts[6] = { P(4, 6), P(10, 3), P(17, 5), P(16, 13), P(9, 17), P(3, 12) };
        g.DrawClosedCurve(&pen, pts, 6);
        break;
    }
    case PI_CROP:
    {
        PointF a[3] = { P(6, 1.5f), P(6, 14), P(18.5f, 14) };
        PointF b[3] = { P(1.5f, 6), P(14, 6), P(14, 18.5f) };
        g.DrawLines(&pen, a, 3);
        g.DrawLines(&pen, b, 3);
        break;
    }
    case PI_RESIZE:
    {
        g.DrawRectangle(&pen, R(2.5f, 9.5f, 8, 8));
        g.DrawLine(&pen, P(8, 12), P(17, 3));
        PointF h[3] = { P(12, 3), P(17, 3), P(17, 8) };
        g.DrawLines(&pen, h, 3);
        break;
    }
    case PI_ROTATE:
    {
        g.DrawArc(&pen, R(3, 4, 14, 14), 0, 270);
        PointF h[3] = { P(7, 1), P(10, 4), P(7, 7) };
        g.DrawLines(&pen, h, 3);
        break;
    }
    case PI_PENCIL:
    {
        PointF body[5] = { P(13.5f, 2.5f), P(17.5f, 6.5f), P(7, 17), P(3, 17), P(3, 13) };
        g.DrawPolygon(&pen, body, 5);
        g.DrawLine(&pen, P(11.5f, 4.5f), P(15.5f, 8.5f));
        break;
    }
    case PI_FILL:
    {
        PointF body[4] = { P(2.5f, 10), P(9, 3.5f), P(15.5f, 10), P(9, 16.5f) };
        g.DrawPolygon(&pen, body, 4);
        g.DrawLine(&pen, P(9, 3.5f), P(6, 0.8f));
        g.DrawLine(&pen, P(2.5f, 10), P(15.5f, 10));
        g.FillEllipse(&br, R(15, 12.5f, 4, 5.5f));
        break;
    }
    case PI_TEXT:
    {
        PointF a[3] = { P(4, 17), P(10, 3), P(16, 17) };
        g.DrawLines(&pen, a, 3);
        g.DrawLine(&pen, P(6.5f, 12), P(13.5f, 12));
        break;
    }
    case PI_ERASER:
    {
        PointF body[5] = { P(7.5f, 17), P(2.5f, 12), P(11, 3.5f), P(18, 10.5f), P(11.5f, 17) };
        g.DrawPolygon(&pen, body, 5);
        g.DrawLine(&pen, P(6.5f, 7.5f), P(13.5f, 14.5f));
        g.DrawLine(&pen, P(11.5f, 17), P(18, 17));
        break;
    }
    case PI_PICKER:
    {
        g.DrawLine(&pen, P(3, 17), P(11.5f, 8.5f));
        g.FillEllipse(&br, R(11, 2, 7, 7));
        g.DrawLine(&pen, P(9, 5), P(15, 11));
        break;
    }
    case PI_MAGNIFIER:
    {
        g.DrawEllipse(&pen, R(2.5f, 2.5f, 10.5f, 10.5f));
        Pen thick(c, 2.4f * k);
        thick.SetStartCap(LineCapRound);
        thick.SetEndCap(LineCapRound);
        g.DrawLine(&thick, P(12, 12), P(17.5f, 17.5f));
        break;
    }
    case PI_BRUSH:
    {
        Pen thick(c, 2.2f * k);
        thick.SetStartCap(LineCapRound);
        thick.SetEndCap(LineCapRound);
        g.DrawLine(&thick, P(17.5f, 2.5f), P(10.5f, 9.5f));
        PointF tip[5] = { P(8.5f, 8.8f), P(11.2f, 11.5f), P(8.5f, 16), P(2, 18), P(4, 12.5f) };
        g.FillClosedCurve(&br, tip, 5, FillModeAlternate, 0.3f);
        break;
    }
    case PI_SIZE:
    {
        for (int i = 0; i < 4; ++i)
        {
            Pen p(c, (1.0f + i) * k);
            float y = 3.5f + i * 4.5f;
            g.DrawLine(&p, P(2.5f, y), P(17.5f, y));
        }
        break;
    }
    case PI_OUTLINE:
    {
        Pen p(c, 1.8f * k);
        g.DrawRectangle(&p, R(3, 4, 14, 12));
        break;
    }
    case PI_FILLMODE:
    {
        g.FillRectangle(&br, R(3, 4, 14, 12));
        break;
    }
    case PI_EDITCOLOR:
    {
        static const Color hues[6] = { Color(255, 237, 28, 36), Color(255, 255, 201, 14), Color(255, 34, 177, 76),
                                       Color(255, 0, 162, 232), Color(255, 63, 72, 204), Color(255, 163, 73, 164) };
        for (int i = 0; i < 6; ++i)
        {
            SolidBrush hb(hues[i]);
            g.FillPie(&hb, R(1.5f, 1.5f, 17, 17), -90.0f + i * 60.0f, 60.0f);
        }
        SolidBrush white(Color(255, 255, 255, 255));
        g.FillEllipse(&white, R(7, 7, 6, 6));
        break;
    }
    case PI_BOLD:      DrawLetter(g, r, L"B", FontStyleBold, c); break;
    case PI_ITALIC:    DrawLetter(g, r, L"I", FontStyleItalic, c); break;
    case PI_UNDERLINE: DrawLetter(g, r, L"U", FontStyleUnderline, c); break;
    case PI_FONT:      DrawLetter(g, r, L"A", FontStyleRegular, c); break;
    case PI_OPAQUE:
    {
        SolidBrush bg(Color(255, 205, 210, 218));
        g.FillRectangle(&bg, R(2, 3, 16, 14));
        DrawLetter(g, R(2, 3, 16, 14), L"A", FontStyleBold, c);
        break;
    }
    default:
        break;
    }
}
