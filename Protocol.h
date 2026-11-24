#pragma once
#include "framework.h"

// 공유 캔버스 통신 프로토콜
// 메시지 = [u32 길이(종류 1바이트 + 내용)] [u8 종류] [내용]. 정수는 리틀 엔디언, 문자열은 [u16 길이][UTF-8].

namespace Proto
{
    const DWORD MAGIC = 0x54505753;          // "SWPT"
    const WORD VERSION = 1;
    const DWORD MAX_MESSAGE = 128u * 1024 * 1024;
    const UINT DEFAULT_PORT = 52700;
    const int MAX_ID_LENGTH = 20;

    enum Type : BYTE
    {
        HELLO = 1,       // 클라 → 서버: magic, version, secret[8], id
        WELCOME = 2,     // 서버 → 클라: 내 색, 참가자 목록, 전체 그림
        REJECT = 3,      // 서버 → 클라: 거부 사유 코드, 메시지
        USER_JOIN = 4,   // 서버 → 클라: id, 색
        USER_LEAVE = 5,  // 서버 → 클라: id
        EDIT = 6,        // 양방향: 작성자, 사각형, 압축된 픽셀
        FULL = 7,        // 양방향: 작성자, 전체 그림 (크기 변경 시)
        CURSOR = 8,      // 양방향: id, x, y, 표시 여부
        CHAT = 9,        // 양방향: id, 내용
    };

    enum RejectCode : BYTE
    {
        REJECT_DUPLICATE_ID = 1,
        REJECT_BAD_KEY = 2,
        REJECT_VERSION = 3,
        REJECT_FULL = 4,
        REJECT_BAD_ID = 5,
    };

    class Writer
    {
    public:
        explicit Writer(Type t);
        void U8(BYTE v) { m_b.push_back(v); }
        void U16(WORD v);
        void U32(DWORD v);
        void I32(int v) { U32((DWORD)v); }
        void Str(const CString& s);
        void Bytes(const void* p, size_t n);
        // 길이 머리를 채워 전송용 버퍼를 돌려준다
        std::vector<BYTE> Finish();
    private:
        std::vector<BYTE> m_b;
    };

    class Reader
    {
    public:
        Reader(const BYTE* p, size_t n) : m_p(p), m_n(n) {}
        BYTE U8();
        WORD U16();
        DWORD U32();
        int I32() { return (int)U32(); }
        CString Str();
        const BYTE* Bytes(size_t n);
        bool Ok() const { return m_ok; }
    private:
        const BYTE* m_p;
        size_t m_n, m_pos = 0;
        bool m_ok = true;
        bool Need(size_t n);
    };

    // 픽셀(0x00RRGGBB) 영역을 BGR로 묶어 압축 / 해제
    bool CompressPixels(const DWORD* src, int w, int h, int stride, std::vector<BYTE>& out);
    bool DecompressPixels(const BYTE* data, size_t len, int w, int h, std::vector<DWORD>& out);

    // 초대 키: IPv4 + 포트 + 비밀값 8바이트 + 검사값 → Base32 24글자 (XXXX-XXXX-...)
    CString EncodeInviteKey(DWORD ipv4HostOrder, WORD port, const BYTE secret[8]);
    bool DecodeInviteKey(const CString& key, DWORD& ipv4HostOrder, WORD& port, BYTE secret[8]);
    CString IpToString(DWORD ipv4HostOrder);
    void RandomBytes(BYTE* p, size_t n);

    // 이 PC의 IPv4 주소 목록 (표시 이름 포함), 마지막은 127.0.0.1
    struct LocalAddress { DWORD ip; CString label; };
    std::vector<LocalAddress> EnumLocalAddresses();

    // 참가자 색
    COLORREF UserColor(int index);
    const int USER_COLOR_COUNT = 10;

    bool IsValidUserId(const CString& id);
}
