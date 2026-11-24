// Protocol.cpp : 메시지 인코딩, 픽셀 압축, 초대 키, 로컬 주소
#include "Protocol.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <compressapi.h>
#include <bcrypt.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "cabinet.lib")
#pragma comment(lib, "bcrypt.lib")

namespace Proto
{

// ---------------------------------------------------------------------------
// Writer / Reader

Writer::Writer(Type t)
{
    m_b.resize(4);      // 길이 자리
    m_b.push_back((BYTE)t);
}

void Writer::U16(WORD v)
{
    m_b.push_back((BYTE)(v & 0xFF));
    m_b.push_back((BYTE)(v >> 8));
}

void Writer::U32(DWORD v)
{
    for (int i = 0; i < 4; ++i)
        m_b.push_back((BYTE)((v >> (8 * i)) & 0xFF));
}

void Writer::Str(const CString& s)
{
    CStringA utf8 = CW2A(s, CP_UTF8);
    int n = min(utf8.GetLength(), 0xFFFF);
    U16((WORD)n);
    Bytes((LPCSTR)utf8, n);
}

void Writer::Bytes(const void* p, size_t n)
{
    const BYTE* b = (const BYTE*)p;
    m_b.insert(m_b.end(), b, b + n);
}

std::vector<BYTE> Writer::Finish()
{
    DWORD len = (DWORD)(m_b.size() - 4);
    for (int i = 0; i < 4; ++i)
        m_b[i] = (BYTE)((len >> (8 * i)) & 0xFF);
    return std::move(m_b);
}

bool Reader::Need(size_t n)
{
    if (!m_ok || m_n - m_pos < n)
    {
        m_ok = false;
        return false;
    }
    return true;
}

BYTE Reader::U8()
{
    if (!Need(1)) return 0;
    return m_p[m_pos++];
}

WORD Reader::U16()
{
    if (!Need(2)) return 0;
    WORD v = (WORD)(m_p[m_pos] | (m_p[m_pos + 1] << 8));
    m_pos += 2;
    return v;
}

DWORD Reader::U32()
{
    if (!Need(4)) return 0;
    DWORD v = 0;
    for (int i = 0; i < 4; ++i)
        v |= (DWORD)m_p[m_pos + i] << (8 * i);
    m_pos += 4;
    return v;
}

CString Reader::Str()
{
    WORD n = U16();
    const BYTE* p = Bytes(n);
    if (!p) return CString();
    CStringA a((LPCSTR)p, n);
    return CString(CA2W(a, CP_UTF8));
}

const BYTE* Reader::Bytes(size_t n)
{
    if (!Need(n)) return nullptr;
    const BYTE* p = m_p + m_pos;
    m_pos += n;
    return p;
}

// ---------------------------------------------------------------------------
// 압축 (Windows Compression API, XPRESS+Huffman)

bool CompressPixels(const DWORD* src, int w, int h, int stride, std::vector<BYTE>& out)
{
    out.clear();
    if (w <= 0 || h <= 0)
        return false;
    std::vector<BYTE> raw((size_t)w * h * 3);
    BYTE* d = raw.data();
    for (int y = 0; y < h; ++y)
    {
        const DWORD* row = src + (size_t)y * stride;
        for (int x = 0; x < w; ++x)
        {
            DWORD p = row[x];
            *d++ = (BYTE)(p & 0xFF);
            *d++ = (BYTE)((p >> 8) & 0xFF);
            *d++ = (BYTE)((p >> 16) & 0xFF);
        }
    }
    COMPRESSOR_HANDLE hc = nullptr;
    if (!::CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &hc))
        return false;
    SIZE_T need = 0;
    ::Compress(hc, raw.data(), raw.size(), nullptr, 0, &need);
    out.resize(need);
    SIZE_T got = 0;
    bool ok = ::Compress(hc, raw.data(), raw.size(), out.data(), out.size(), &got) != FALSE;
    ::CloseCompressor(hc);
    if (!ok)
        return false;
    out.resize(got);
    return true;
}

bool DecompressPixels(const BYTE* data, size_t len, int w, int h, std::vector<DWORD>& out)
{
    if (w <= 0 || h <= 0 || (size_t)w * h > 400u * 1000 * 1000)
        return false;
    std::vector<BYTE> raw((size_t)w * h * 3);
    DECOMPRESSOR_HANDLE hd = nullptr;
    if (!::CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &hd))
        return false;
    SIZE_T got = 0;
    bool ok = ::Decompress(hd, data, len, raw.data(), raw.size(), &got) != FALSE;
    ::CloseDecompressor(hd);
    if (!ok || got != raw.size())
        return false;
    out.resize((size_t)w * h);
    const BYTE* s = raw.data();
    for (size_t i = 0; i < out.size(); ++i, s += 3)
        out[i] = 0xFF000000u | ((DWORD)s[2] << 16) | ((DWORD)s[1] << 8) | s[0];
    return true;
}

// ---------------------------------------------------------------------------
// 초대 키

static const char kB32[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";   // Crockford Base32

static BYTE Checksum(const BYTE* p, size_t n)
{
    BYTE c = 0x5A;
    for (size_t i = 0; i < n; ++i)
        c = (BYTE)((c << 1 | c >> 7) ^ p[i]);
    return c;
}

CString EncodeInviteKey(DWORD ip, WORD port, const BYTE secret[8])
{
    BYTE raw[15];
    raw[0] = (BYTE)(ip >> 24); raw[1] = (BYTE)(ip >> 16); raw[2] = (BYTE)(ip >> 8); raw[3] = (BYTE)ip;
    raw[4] = (BYTE)(port >> 8); raw[5] = (BYTE)port;
    memcpy(raw + 6, secret, 8);
    raw[14] = Checksum(raw, 14);

    // 120비트 → 24글자
    CString out;
    int bits = 0;
    unsigned acc = 0;
    int chars = 0;
    for (BYTE b : raw)
    {
        acc = (acc << 8) | b;
        bits += 8;
        while (bits >= 5)
        {
            bits -= 5;
            if (chars > 0 && chars % 4 == 0)
                out += _T('-');
            out += (TCHAR)kB32[(acc >> bits) & 31];
            ++chars;
        }
    }
    return out;
}

static int B32Value(TCHAR c)
{
    c = (TCHAR)_totupper(c);
    if (c == _T('O')) c = _T('0');
    if (c == _T('I') || c == _T('L')) c = _T('1');
    for (int i = 0; i < 32; ++i)
        if ((TCHAR)kB32[i] == c)
            return i;
    return -1;
}

bool DecodeInviteKey(const CString& key, DWORD& ip, WORD& port, BYTE secret[8])
{
    BYTE raw[15] = {};
    int n = 0, bits = 0;
    unsigned acc = 0;
    for (int i = 0; i < key.GetLength(); ++i)
    {
        TCHAR c = key[i];
        if (c == _T('-') || c == _T(' ') || c == _T('\t'))
            continue;
        int v = B32Value(c);
        if (v < 0)
            return false;
        acc = (acc << 5) | (unsigned)v;
        bits += 5;
        if (bits >= 8)
        {
            bits -= 8;
            if (n >= 15)
                return false;
            raw[n++] = (BYTE)((acc >> bits) & 0xFF);
        }
    }
    if (n != 15 || raw[14] != Checksum(raw, 14))
        return false;
    ip = ((DWORD)raw[0] << 24) | ((DWORD)raw[1] << 16) | ((DWORD)raw[2] << 8) | raw[3];
    port = (WORD)((raw[4] << 8) | raw[5]);
    memcpy(secret, raw + 6, 8);
    return port != 0;
}

CString IpToString(DWORD ip)
{
    CString s;
    s.Format(_T("%u.%u.%u.%u"), (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
    return s;
}

void RandomBytes(BYTE* p, size_t n)
{
    if (!BCRYPT_SUCCESS(::BCryptGenRandom(nullptr, p, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
    {
        for (size_t i = 0; i < n; ++i)
            p[i] = (BYTE)(rand() ^ (::GetTickCount() >> (i % 8)));
    }
}

// ---------------------------------------------------------------------------
// 로컬 주소

std::vector<LocalAddress> EnumLocalAddresses()
{
    std::vector<LocalAddress> out;
    ULONG size = 16 * 1024;
    std::vector<BYTE> buf(size);
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG r = ::GetAdaptersAddresses(AF_INET, flags, nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &size);
    if (r == ERROR_BUFFER_OVERFLOW)
    {
        buf.resize(size);
        r = ::GetAdaptersAddresses(AF_INET, flags, nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &size);
    }
    if (r == NO_ERROR)
    {
        for (auto a = (PIP_ADAPTER_ADDRESSES)buf.data(); a; a = a->Next)
        {
            if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;
            for (auto u = a->FirstUnicastAddress; u; u = u->Next)
            {
                if (u->Address.lpSockaddr->sa_family != AF_INET)
                    continue;
                auto sin = (sockaddr_in*)u->Address.lpSockaddr;
                DWORD ip = ntohl(sin->sin_addr.s_addr);
                if ((ip >> 16) == 0xA9FE)       // 169.254.x.x (자동 할당) 제외
                    continue;
                LocalAddress la;
                la.ip = ip;
                la.label.Format(_T("%s  (%s)"), (LPCTSTR)IpToString(ip), a->FriendlyName);
                out.push_back(la);
            }
        }
    }
    LocalAddress lo;
    lo.ip = 0x7F000001;
    lo.label = _T("127.0.0.1  (이 PC에서만 - 테스트용)");
    out.push_back(lo);
    return out;
}

COLORREF UserColor(int index)
{
    static const COLORREF colors[USER_COLOR_COUNT] =
    {
        RGB(0, 120, 215), RGB(232, 17, 35), RGB(16, 137, 62), RGB(202, 80, 16), RGB(136, 23, 152),
        RGB(0, 153, 188), RGB(194, 57, 179), RGB(107, 105, 214), RGB(132, 117, 69), RGB(76, 74, 72),
    };
    return colors[((index % USER_COLOR_COUNT) + USER_COLOR_COUNT) % USER_COLOR_COUNT];
}

bool IsValidUserId(const CString& id)
{
    CString t = id;
    t.Trim();
    if (t.IsEmpty() || t != id || t.GetLength() > MAX_ID_LENGTH)
        return false;
    for (int i = 0; i < t.GetLength(); ++i)
        if (t[i] < 0x20)
            return false;
    return true;
}

}  // namespace Proto
