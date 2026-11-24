#pragma once
#include "framework.h"

// 픽셀 형식: 0xAARRGGBB (32bpp top-down DIB)
inline DWORD ToPixel(COLORREF c)
{
    return 0xFF000000u | ((DWORD)GetRValue(c) << 16) | ((DWORD)GetGValue(c) << 8) | (DWORD)GetBValue(c);
}
inline COLORREF ToColor(DWORD p)
{
    return RGB((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
}

// 32비트 DIB 섹션 래퍼. GDI 그리기와 직접 픽셀 접근을 모두 지원한다.
// 주 이미지에서는 알파를 무시하고, 떠 있는 선택 영역에서는 알파를 마스크로 사용한다.
class CBitmap32
{
public:
    CBitmap32() {}
    CBitmap32(const CBitmap32& o);
    CBitmap32(CBitmap32&& o) noexcept;
    CBitmap32& operator=(const CBitmap32& o);
    CBitmap32& operator=(CBitmap32&& o) noexcept;
    ~CBitmap32() { Destroy(); }

    bool Create(int w, int h, DWORD fill = 0xFFFFFFFF);
    void Destroy();

    bool IsNull() const { return m_hbm == nullptr; }
    int Width() const { return m_w; }
    int Height() const { return m_h; }
    CSize Size() const { return CSize(m_w, m_h); }
    HBITMAP Handle() const { return m_hbm; }

    DWORD* Row(int y) { return m_bits + (size_t)y * m_w; }
    const DWORD* Row(int y) const { return m_bits + (size_t)y * m_w; }
    bool InBounds(int x, int y) const { return x >= 0 && y >= 0 && x < m_w && y < m_h; }
    DWORD GetPixel(int x, int y) const { return m_bits[(size_t)y * m_w + x]; }
    void SetPixel(int x, int y, DWORD p) { m_bits[(size_t)y * m_w + x] = p; }

    void Fill(DWORD p);
    void FillRect(CRect rc, DWORD p);
    CBitmap32 Crop(const CRect& rc, DWORD fill) const;
    CBitmap32 FlipH() const;
    CBitmap32 FlipV() const;
    CBitmap32 Rotate90(bool clockwise) const;
    CBitmap32 Rotate180() const;
    CBitmap32 Scale(int w, int h) const;
    CBitmap32 Skew(double degH, double degV, DWORD fill) const;
    void InvertRGB();
    void SetAlphaAll(BYTE a);

private:
    HBITMAP m_hbm = nullptr;
    DWORD* m_bits = nullptr;
    int m_w = 0, m_h = 0;
};

// CBitmap32를 선택한 메모리 DC (RAII). 소멸 시 GdiFlush 후 해제한다.
class CBitmapDC
{
public:
    explicit CBitmapDC(CBitmap32& bmp);
    ~CBitmapDC();
    CDC* operator->() { return &m_dc; }
    CDC& DC() { return m_dc; }
    HDC GetSafeHdc() const { return m_dc.GetSafeHdc(); }
private:
    CDC m_dc;
    HGDIOBJ m_old = nullptr;
};
