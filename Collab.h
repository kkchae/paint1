#pragma once
#include "framework.h"
#include <afxsock.h>
#include "Protocol.h"
#include "Bitmap32.h"

// 공유 세션이 화면 쪽(메인 프레임)에 알리는 이벤트
class ICollabEvents
{
public:
    virtual ~ICollabEvents() {}
    virtual void OnCollabStateChanged() = 0;                                   // 역할/연결/참가자 변경
    virtual void OnCollabChat(const CString& id, COLORREF color, const CString& text, bool system) = 0;
    virtual void OnCollabEdit(const CRect& rc, const std::vector<DWORD>& px) = 0;
    virtual void OnCollabFull(int w, int h, const std::vector<DWORD>& px) = 0;
    virtual void OnCollabCursor(CPoint oldPos, bool oldVisible, CPoint pos, bool visible) = 0;
    virtual void OnCollabJoined() = 0;                                         // 클라: 참가 완료
    virtual void OnCollabFailed(const CString& message) = 0;                   // 클라: 거부/연결 실패/끊김
    virtual void CollabFlushPending() = 0;                                     // 호스트: 보내지 않은 편집을 먼저 보낸다
    virtual const CBitmap32& CollabCurrentImage() = 0;                         // 호스트: 새 참가자에게 줄 그림
    virtual void CollabRequestCleanup() = 0;                                   // 닫힌 소켓 정리 예약
};

struct CollabUser
{
    CString id;
    COLORREF color = 0;
    int colorIndex = 0;
    bool isMe = false;
    bool isHost = false;
    CPoint cursor;
    bool cursorVisible = false;
};

class CCollabSession;

class CCollabSocket : public CAsyncSocket
{
public:
    CCollabSession* m_owner = nullptr;
    bool m_dead = false;
    bool m_closeWhenFlushed = false;
    bool m_authed = false;
    CString m_id;
    COLORREF m_color = 0;
    DWORD m_created = 0;

    void Queue(const std::vector<BYTE>& msg);
    void Flush();

protected:
    std::vector<BYTE> m_in, m_out;
    size_t m_inPos = 0, m_outPos = 0;

    void OnReceive(int nErrorCode) override;
    void OnSend(int nErrorCode) override;
    void OnClose(int nErrorCode) override;
    void OnConnect(int nErrorCode) override;
};

class CCollabListenSocket : public CAsyncSocket
{
public:
    CCollabSession* m_owner = nullptr;
protected:
    void OnAccept(int nErrorCode) override;
};

class CCollabSession
{
public:
    enum Role { ROLE_NONE, ROLE_HOST, ROLE_CLIENT };
    enum State { STATE_IDLE, STATE_CONNECTING, STATE_ACTIVE };

    explicit CCollabSession(ICollabEvents* events);
    ~CCollabSession();

    bool StartHost(const CString& myId, DWORD ip, UINT port, CString& err);
    bool Join(const CString& inviteKey, const CString& myId, CString& err);
    void Stop();

    Role GetRole() const { return m_role; }
    State GetState() const { return m_state; }
    bool IsActive() const { return m_state == STATE_ACTIVE; }
    const CString& InviteKey() const { return m_key; }
    const CString& MyId() const { return m_myId; }
    CString HostAddress() const;
    const std::vector<CollabUser>& Users() const { return m_users; }

    // 내 쪽에서 보내는 것
    void SendEdit(const CBitmap32& img, const CRect& rc);
    void SendFull(const CBitmap32& img);
    void SendCursor(CPoint p, bool visible);
    void SendChat(const CString& text);

    void Tick();        // 1초마다: 연결/인사 시간 초과
    void Cleanup();     // 닫힌 소켓 삭제

    // 소켓 콜백
    void OnAccept();
    void OnConnected(CCollabSocket* s, int err);
    void OnSocketClosed(CCollabSocket* s);
    bool OnMessage(CCollabSocket* s, BYTE type, Proto::Reader& r);

private:
    ICollabEvents* m_events;
    Role m_role = ROLE_NONE;
    State m_state = STATE_IDLE;
    CString m_myId, m_key;
    DWORD m_hostIp = 0;
    UINT m_port = 0;
    BYTE m_secret[8] = {};
    DWORD m_connectStart = 0;

    CCollabListenSocket* m_listen = nullptr;
    std::vector<CCollabSocket*> m_clients;     // 호스트: 참가자 연결
    CCollabSocket* m_server = nullptr;         // 클라: 호스트 연결
    std::vector<CCollabSocket*> m_dead;
    std::vector<CollabUser> m_users;

    void MarkDead(CCollabSocket* s);
    CollabUser* FindUser(const CString& id);
    void RemoveUser(const CString& id);
    void Broadcast(const std::vector<BYTE>& msg, CCollabSocket* except = nullptr);
    void SendToHostOrAll(const std::vector<BYTE>& msg);
    void Reject(CCollabSocket* s, Proto::RejectCode code, const CString& message);
    bool HostHandle(CCollabSocket* s, BYTE type, Proto::Reader& r);
    bool ClientHandle(BYTE type, Proto::Reader& r);
    void Fail(const CString& message);
    static std::vector<BYTE> MakeImageMsg(Proto::Type type, const CString& author, const CBitmap32& img, const CRect& rc);
};
