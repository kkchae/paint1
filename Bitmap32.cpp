#include "Bitmap32.h"
#include <algorithm>

static const double PI = 3.14159265358979323846;

CBitmap32::CBitmap32(const CBitmap32& o)
{
    *this = o;
}

CBitmap32::CBitmap32(CBitmap32&& o) noexcept
{
    *this = std::move(o);
}

CBitmap32& CBitmap32::operator=(const CBitmap32& o)
{
    if (this == &o)
        return *this;
    Destroy();
    if (!o.IsNull() && Create(o.m_w, o.m_h, 0))
    {
        ::GdiFlush();
        memcpy(m_bits, o.m_bits, (size_t)m_w * m_h * 4);
    }
    return *this;
}

CBitmap32& CBitmap32::operator=(CBitmap32&& o) noexcept
{
    if (this != &o)
    {
        Destroy();
        m_hbm = o.m_hbm; m_bits = o.m_bits; m_w = o.m_w; m_h = o.m_h;
        o.m_hbm = nullptr; o.m_bits = nullptr; o.m_w = o.m_h = 0;
    }
    return *this;
}

bool CBitmap32::Create(int w, int h, DWORD fill)
{
    Destroy();
    if (w <= 0 || h <= 0)
        return false;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;     // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    m_hbm = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!m_hbm)
        return false;
    m_bits = (DWORD*)bits;
    m_w = w;
    m_h = h;
    Fill(fill);
    return true;
}

void CBitmap32::Destroy()
{
    if (m_hbm)
        ::DeleteObject(m_hbm);
    m_hbm = nullptr;
    m_bits = nullptr;
    m_w = m_h = 0;
}

void CBitmap32::Fill(DWORD p)
{
    if (!m_bits) return;
    ::GdiFlush();
    std::fill(m_bits, m_bits + (size_t)m_w * m_h, p);
}

void CBitmap32::FillRect(CRect rc, DWORD p)
{
    if (!m_bits) return;
    CRect bounds(0, 0, m_w, m_h);
    if (!rc.IntersectRect(rc, bounds))
        return;
    for (int y = rc.top; y < rc.bottom; ++y)
    {
        DWORD* row = Row(y);
        std::fill(row + rc.left, row + rc.right, p);
    }
}

CBitmap32 CBitmap32::Crop(const CRect& rc, DWORD fill) const
{
    CBitmap32 r;
    if (!r.Create(rc.Width(), rc.Height(), fill) || IsNull())
        return r;
    ::GdiFlush();
    CRect src;
    if (!src.IntersectRect(rc, CRect(0, 0, m_w, m_h)))
        return r;
    for (int y = src.top; y < src.bottom; ++y)
        memcpy(r.Row(y - rc.top) + (src.left - rc.left), Row(y) + src.left, (size_t)src.Width() * 4);
    return r;
}

CBitmap32 CBitmap32::FlipH() const
{
    CBitmap32 r;
    if (!r.Create(m_w, m_h, 0)) return r;
    ::GdiFlush();
    for (int y = 0; y < m_h; ++y)
    {
        const DWORD* s = Row(y);
        DWORD* d = r.Row(y);
        for (int x = 0; x < m_w; ++x)
            d[x] = s[m_w - 1 - x];
    }
    return r;
}

CBitmap32 CBitmap32::FlipV() const
{
    CBitmap32 r;
    if (!r.Create(m_w, m_h, 0)) return r;
    ::GdiFlush();
    for (int y = 0; y < m_h; ++y)
        memcpy(r.Row(y), Row(m_h - 1 - y), (size_t)m_w * 4);
    return r;
}

CBitmap32 CBitmap32::Rotate90(bool clockwise) const
{
    CBitmap32 r;
    if (!r.Create(m_h, m_w, 0)) return r;
    ::GdiFlush();
    for (int y = 0; y < m_h; ++y)
    {
        const DWORD* s = Row(y);
        for (int x = 0; x < m_w; ++x)
        {
            if (clockwise)
                r.SetPixel(m_h - 1 - y, x, s[x]);
            else
                r.SetPixel(y, m_w - 1 - x, s[x]);
        }
    }
    return r;
}

CBitmap32 CBitmap32::Rotate180() const
{
    CBitmap32 r;
    if (!r.Create(m_w, m_h, 0)) return r;
    ::GdiFlush();
    for (int y = 0; y < m_h; ++y)
    {
        const DWORD* s = Row(y);
        DWORD* d = r.Row(m_h - 1 - y);
        for (int x = 0; x < m_w; ++x)
            d[m_w - 1 - x] = s[x];
    }
    return r;
}

// 양선형 보간 크기 조정 (알파 채널 포함)
CBitmap32 CBitmap32::Scale(int w, int h) const
{
    CBitmap32 r;
    if (IsNull() || !r.Create(w, h, 0)) return r;
    ::GdiFlush();
    double sx = (double)m_w / w, sy = (double)m_h / h;
    for (int y = 0; y < h; ++y)
    {
        double fy = (y + 0.5) * sy - 0.5;
        if (fy < 0) fy = 0;
        int y0 = (int)fy;
        if (y0 > m_h - 1) y0 = m_h - 1;
        int y1 = y0 + 1 < m_h ? y0 + 1 : m_h - 1;
        double ty = fy - y0;
        DWORD* d = r.Row(y);
        for (int x = 0; x < w; ++x)
        {
            double fx = (x + 0.5) * sx - 0.5;
            if (fx < 0) fx = 0;
            int x0 = (int)fx;
            if (x0 > m_w - 1) x0 = m_w - 1;
            int x1 = x0 + 1 < m_w ? x0 + 1 : m_w - 1;
            double tx = fx - x0;
            DWORD p00 = GetPixel(x0, y0), p10 = GetPixel(x1, y0);
            DWORD p01 = GetPixel(x0, y1), p11 = GetPixel(x1, y1);
            DWORD out = 0;
            for (int sh = 0; sh < 32; sh += 8)
            {
                double c00 = (p00 >> sh) & 0xFF, c10 = (p10 >> sh) & 0xFF;
                double c01 = (p01 >> sh) & 0xFF, c11 = (p11 >> sh) & 0xFF;
                double top = c00 + (c10 - c00) * tx;
                double bot = c01 + (c11 - c01) * tx;
                int v = (int)(top + (bot - top) * ty + 0.5);
                out |= (DWORD)ClampV(v, 0, 255) << sh;
            }
            d[x] = out;
        }
    }
    return r;
}

// 가로/세로 기울이기 (각도 단위: 도)
CBitmap32 CBitmap32::Skew(double degH, double degV, DWORD fill) const
{
    if (IsNull()) return CBitmap32();
    ::GdiFlush();
    double th = tan(degH * PI / 180.0);
    double tv = tan(degV * PI / 180.0);

    // 1단계: 가로 기울이기  x' = x + th * y
    int w1 = m_w + (int)ceil(fabs(th) * m_h);
    int h1 = m_h;
    double offX = th < 0 ? -th * m_h : 0.0;
    CBitmap32 a;
    if (!a.Create(w1, h1, fill)) return a;
    for (int y = 0; y < h1; ++y)
    {
        DWORD* d = a.Row(y);
        const DWORD* s = Row(y);
        double shift = offX + th * (y + 0.5);
        for (int x = 0; x < w1; ++x)
        {
            int sxp = (int)floor(x + 0.5 - shift);
            if (sxp >= 0 && sxp < m_w)
                d[x] = s[sxp];
        }
    }
    if (fabs(tv) < 1e-9)
        return a;

    // 2단계: 세로 기울이기  y' = y + tv * x
    int w2 = w1;
    int h2 = h1 + (int)ceil(fabs(tv) * w1);
    double offY = tv < 0 ? -tv * w1 : 0.0;
    CBitmap32 b;
    if (!b.Create(w2, h2, fill)) return b;
    for (int x = 0; x < w2; ++x)
    {
        double shift = offY + tv * (x + 0.5);
        for (int y = 0; y < h2; ++y)
        {
            int syp = (int)floor(y + 0.5 - shift);
            if (syp >= 0 && syp < h1)
                b.SetPixel(x, y, a.GetPixel(x, syp));
        }
    }
    return b;
}

void CBitmap32::InvertRGB()
{
    if (!m_bits) return;
    ::GdiFlush();
    size_t n = (size_t)m_w * m_h;
    for (size_t i = 0; i < n; ++i)
        m_bits[i] ^= 0x00FFFFFF;
}

void CBitmap32::SetAlphaAll(BYTE a)
{
    if (!m_bits) return;
    ::GdiFlush();
    size_t n = (size_t)m_w * m_h;
    DWORD al = (DWORD)a << 24;
    for (size_t i = 0; i < n; ++i)
        m_bits[i] = (m_bits[i] & 0x00FFFFFF) | al;
}

// ---------------------------------------------------------------------------

CBitmapDC::CBitmapDC(CBitmap32& bmp)
{
    m_dc.CreateCompatibleDC(nullptr);
    m_old = ::SelectObject(m_dc.GetSafeHdc(), bmp.Handle());
}

CBitmapDC::~CBitmapDC()
{
    ::GdiFlush();
    if (m_dc.GetSafeHdc())
    {
        ::SelectObject(m_dc.GetSafeHdc(), m_old);
        m_dc.DeleteDC();
    }
}
