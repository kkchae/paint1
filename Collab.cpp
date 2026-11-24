// Collab.cpp : 공유 캔버스 세션 (호스트 = 서버, 참가자 = 클라이언트)
//
// 호스트가 편집 순서를 정한다. 참가자의 편집은 호스트가 반영한 뒤 보낸 사람을 포함한
// 모든 참가자에게 다시 보내므로, 동시에 같은 곳을 고쳐도 모든 그림이 같은 결과가 된다.
#include "Collab.h"
#include <algorithm>

using namespace Proto;

static const size_t MAX_OUT_BUFFER = 256u * 1024 * 1024;
static const DWORD CONNECT_TIMEOUT_MS = 10000;
static const DWORD HELLO_TIMEOUT_MS = 10000;
static const int MAX_PARTICIPANTS = 16;

// ---------------------------------------------------------------------------
// 소켓

void CCollabSocket::Queue(const std::vector<BYTE>& msg)
{
    if (m_dead)
        return;
    if (m_out.size() - m_outPos + msg.size() > MAX_OUT_BUFFER)
    {
        // 받는 쪽이 너무 느리다: 연결을 끊는다
        m_owner->OnSocketClosed(this);
        return;
    }
    m_out.insert(m_out.end(), msg.begin(), msg.end());
    Flush();
}

void CCollabSocket::Flush()
{
    while (!m_dead && m_outPos < m_out.size())
    {
        int chunk = (int)min((size_t)65536, m_out.size() - m_outPos);
        int n = Send(&m_out[m_outPos], chunk);
        if (n == SOCKET_ERROR)
        {
            if (GetLastError() == WSAEWOULDBLOCK)
                break;      // OnSend 에서 이어서 보낸다
            m_owner->OnSocketClosed(this);
            return;
        }
        m_outPos += (size_t)n;
    }
    if (m_outPos == m_out.size())
    {
        m_out.clear();
        m_outPos = 0;
        if (m_closeWhenFlushed && !m_dead)
        {
            ShutDown(CAsyncSocket::sends);
            m_owner->OnSocketClosed(this);
        }
    }
    else if (m_outPos > 4 * 1024 * 1024)
    {
        m_out.erase(m_out.begin(), m_out.begin() + (ptrdiff_t)m_outPos);
        m_outPos = 0;
    }
}

void CCollabSocket::OnSend(int nErrorCode)
{
    if (nErrorCode == 0)
        Flush();
}

void CCollabSocket::OnConnect(int nErrorCode)
{
    m_owner->OnConnected(this, nErrorCode);
}

void CCollabSocket::OnReceive(int nErrorCode)
{
    if (m_dead || nErrorCode != 0)
        return;
    BYTE buf[65536];
    for (;;)
    {
        int n = Receive(buf, sizeof(buf));
        if (n == SOCKET_ERROR)
        {
            if (GetLastError() != WSAEWOULDBLOCK)
            {
                m_owner->OnSocketClosed(this);
                return;
            }
            break;
        }
        if (n == 0)
            break;
        m_in.insert(m_in.end(), buf, buf + n);
    }

    // 완성된 메시지를 처리
    while (!m_dead)
    {
        size_t avail = m_in.size() - m_inPos;
        if (avail < 5)
            break;
        const BYTE* p = &m_in[m_inPos];
        DWORD len = p[0] | (p[1] << 8) | (p[2] << 16) | ((DWORD)p[3] << 24);
        if (len == 0 || len > MAX_MESSAGE)
        {
            m_owner->OnSocketClosed(this);
            return;
        }
        if (avail < 4 + (size_t)len)
            break;
        BYTE type = p[4];
        Reader r(p + 5, len - 1);
        m_inPos += 4 + (size_t)len;
        if (!m_owner->OnMessage(this, type, r))
        {
            m_owner->OnSocketClosed(this);
            return;
        }
    }
    if (m_inPos > 0 && (m_inPos == m_in.size() || m_inPos > 4 * 1024 * 1024))
    {
        m_in.erase(m_in.begin(), m_in.begin() + (ptrdiff_t)m_inPos);
        m_inPos = 0;
    }
}

void CCollabSocket::OnClose(int)
{
    if (m_dead)
        return;
    OnReceive(0);   // 끊기기 직전에 온 메시지(예: 거부 사유)를 먼저 처리
    if (!m_dead)
        m_owner->OnSocketClosed(this);
}

void CCollabListenSocket::OnAccept(int nErrorCode)
{
    if (nErrorCode == 0)
        m_owner->OnAccept();
}

// ---------------------------------------------------------------------------
// 세션

CCollabSession::CCollabSession(ICollabEvents* events) : m_events(events)
{
}

CCollabSession::~CCollabSession()
{
    m_events = nullptr;
    Stop();
    Cleanup();
}

CString CCollabSession::HostAddress() const
{
    CString s;
    s.Format(_T("%s:%u"), (LPCTSTR)IpToString(m_hostIp), m_port);
    return s;
}

bool CCollabSession::StartHost(const CString& myId, DWORD ip, UINT port, CString& err)
{
    Stop();
    if (!IsValidUserId(myId))
    {
        err.Format(_T("아이디는 1~%d글자이며 앞뒤 공백을 넣을 수 없습니다."), MAX_ID_LENGTH);
        return false;
    }
    m_listen = new CCollabListenSocket;
    m_listen->m_owner = this;
    CString ipStr = IpToString(ip);
    bool ok = false;
    UINT p = port;
    int lastErr = 0;
    for (int i = 0; i < 20 && !ok; ++i, ++p)
    {
        if (m_listen->Create(p, SOCK_STREAM, FD_ACCEPT | FD_CLOSE, ipStr))
        {
            if (m_listen->Listen())
                ok = true;
            else
            {
                lastErr = m_listen->GetLastError();
                m_listen->Close();
            }
        }
        else
            lastErr = ::WSAGetLastError();
    }
    if (!ok)
    {
        delete m_listen;
        m_listen = nullptr;
        err.Format(_T("%s 주소에서 공유를 시작할 수 없습니다. (포트 %u~%u, 오류 %d)"),
                   (LPCTSTR)ipStr, port, port + 19, lastErr);
        return false;
    }
    m_port = p - 1;
    m_hostIp = ip;
    RandomBytes(m_secret, sizeof(m_secret));
    m_key = EncodeInviteKey(ip, (WORD)m_port, m_secret);
    m_myId = myId;
    m_role = ROLE_HOST;
    m_state = STATE_ACTIVE;

    CollabUser me;
    me.id = myId;
    me.colorIndex = 0;
    me.color = UserColor(0);
    me.isMe = me.isHost = true;
    m_users.assign(1, me);
    if (m_events)
        m_events->OnCollabStateChanged();
    return true;
}

bool CCollabSession::Join(const CString& inviteKey, const CString& myId, CString& err)
{
    Stop();
    DWORD ip = 0;
    WORD port = 0;
    BYTE secret[8];
    if (!DecodeInviteKey(inviteKey, ip, port, secret))
    {
        err = _T("초대 키가 올바르지 않습니다. 호스트에게 받은 키를 그대로 붙여넣어 주세요.");
        return false;
    }
    if (!IsValidUserId(myId))
    {
        err.Format(_T("아이디는 1~%d글자이며 앞뒤 공백을 넣을 수 없습니다."), MAX_ID_LENGTH);
        return false;
    }
    m_server = new CCollabSocket;
    m_server->m_owner = this;
    m_server->m_created = ::GetTickCount();
    if (!m_server->Create())
    {
        err.Format(_T("네트워크 소켓을 만들 수 없습니다. (오류 %d)"), ::WSAGetLastError());
        delete m_server;
        m_server = nullptr;
        return false;
    }
    if (!m_server->Connect(IpToString(ip), port) && m_server->GetLastError() != WSAEWOULDBLOCK)
    {
        err.Format(_T("%s:%u 에 연결할 수 없습니다. (오류 %d)"), (LPCTSTR)IpToString(ip), port, m_server->GetLastError());
        delete m_server;
        m_server = nullptr;
        return false;
    }
    memcpy(m_secret, secret, sizeof(m_secret));
    m_hostIp = ip;
    m_port = port;
    m_myId = myId;
    m_role = ROLE_CLIENT;
    m_state = STATE_CONNECTING;
    m_connectStart = ::GetTickCount();
    if (m_events)
        m_events->OnCollabStateChanged();
    return true;
}

void CCollabSession::Stop()
{
    if (m_listen)
    {
        m_listen->Close();
        delete m_listen;    // 대기 소켓은 자기 콜백 밖에서만 멈추므로 바로 지워도 된다
        m_listen = nullptr;
    }
    for (CCollabSocket* s : m_clients)
        MarkDead(s);
    if (m_server)
        MarkDead(m_server);
    bool wasActive = m_role != ROLE_NONE;
    m_role = ROLE_NONE;
    m_state = STATE_IDLE;
    m_users.clear();
    m_key.Empty();
    if (wasActive && m_events)
    {
        m_events->CollabRequestCleanup();
        m_events->OnCollabStateChanged();
    }
}

void CCollabSession::MarkDead(CCollabSocket* s)
{
    if (!s || s->m_dead)
        return;
    s->m_dead = true;
    s->Close();
    m_dead.push_back(s);
    if (m_events)
        m_events->CollabRequestCleanup();
}

void CCollabSession::Cleanup()
{
    for (CCollabSocket* s : m_dead)
    {
        auto it = std::find(m_clients.begin(), m_clients.end(), s);
        if (it != m_clients.end())
            m_clients.erase(it);
        if (m_server == s)
            m_server = nullptr;
        delete s;
    }
    m_dead.clear();
}

void CCollabSession::Tick()
{
    DWORD now = ::GetTickCount();
    if (m_role == ROLE_CLIENT && m_state == STATE_CONNECTING && now - m_connectStart > CONNECT_TIMEOUT_MS)
        Fail(_T("호스트에 연결하지 못했습니다.\n초대 키, 같은 네트워크에 있는지, 호스트 PC의 방화벽 설정을 확인해 주세요."));
    if (m_role == ROLE_HOST)
    {
        for (CCollabSocket* s : m_clients)
            if (!s->m_dead && !s->m_authed && now - s->m_created > HELLO_TIMEOUT_MS)
                MarkDead(s);
    }
}

CollabUser* CCollabSession::FindUser(const CString& id)
{
    for (auto& u : m_users)
        if (u.id.CompareNoCase(id) == 0)
            return &u;
    return nullptr;
}

void CCollabSession::RemoveUser(const CString& id)
{
    for (auto it = m_users.begin(); it != m_users.end(); ++it)
    {
        if (it->id.CompareNoCase(id) == 0)
        {
            CollabUser u = *it;
            m_users.erase(it);
            if (u.cursorVisible && m_events)
                m_events->OnCollabCursor(u.cursor, true, u.cursor, false);
            return;
        }
    }
}

void CCollabSession::Broadcast(const std::vector<BYTE>& msg, CCollabSocket* except)
{
    for (size_t i = 0; i < m_clients.size(); ++i)
    {
        CCollabSocket* s = m_clients[i];
        if (s != except && s->m_authed && !s->m_dead)
            s->Queue(msg);
    }
}

void CCollabSession::SendToHostOrAll(const std::vector<BYTE>& msg)
{
    if (m_state != STATE_ACTIVE)
        return;
    if (m_role == ROLE_HOST)
        Broadcast(msg);
    else if (m_role == ROLE_CLIENT && m_server && !m_server->m_dead)
        m_server->Queue(msg);
}

void CCollabSession::Fail(const CString& message)
{
    Stop();
    if (m_events)
        m_events->OnCollabFailed(message);
}

std::vector<BYTE> CCollabSession::MakeImageMsg(Type type, const CString& author, const CBitmap32& img, const CRect& rc)
{
    std::vector<BYTE> data;
    ::GdiFlush();
    if (!CompressPixels(img.Row(rc.top) + rc.left, rc.Width(), rc.Height(), img.Width(), data))
        return {};
    Writer w(type);
    w.Str(author);
    if (type == EDIT)
    {
        w.I32(rc.left);
        w.I32(rc.top);
    }
    w.I32(rc.Width());
    w.I32(rc.Height());
    w.U32((DWORD)data.size());
    w.Bytes(data.data(), data.size());
    return w.Finish();
}

// ---------------------------------------------------------------------------
// 보내기

void CCollabSession::SendEdit(const CBitmap32& img, const CRect& rc)
{
    if (!IsActive() || rc.IsRectEmpty())
        return;
    auto msg = MakeImageMsg(EDIT, m_myId, img, rc);
    if (!msg.empty())
        SendToHostOrAll(msg);
}

void CCollabSession::SendFull(const CBitmap32& img)
{
    if (!IsActive() || img.IsNull())
        return;
    auto msg = MakeImageMsg(FULL, m_myId, img, CRect(0, 0, img.Width(), img.Height()));
    if (!msg.empty())
        SendToHostOrAll(msg);
}

void CCollabSession::SendCursor(CPoint p, bool visible)
{
    if (!IsActive())
        return;
    Writer w(CURSOR);
    w.Str(m_myId);
    w.I32(p.x);
    w.I32(p.y);
    w.U8(visible ? 1 : 0);
    SendToHostOrAll(w.Finish());
}

void CCollabSession::SendChat(const CString& text)
{
    if (!IsActive() || text.IsEmpty())
        return;
    Writer w(CHAT);
    w.Str(m_myId);
    w.Str(text.Left(2000));
    SendToHostOrAll(w.Finish());
}

// ---------------------------------------------------------------------------
// 연결 이벤트

void CCollabSession::OnAccept()
{
    if (m_role != ROLE_HOST || !m_listen)
        return;
    CCollabSocket* s = new CCollabSocket;
    s->m_owner = this;
    s->m_created = ::GetTickCount();
    if (!m_listen->Accept(*s))
    {
        delete s;
        return;
    }
    s->AsyncSelect(FD_READ | FD_WRITE | FD_CLOSE);
    m_clients.push_back(s);
}

void CCollabSession::OnConnected(CCollabSocket* s, int err)
{
    if (s != m_server || m_state != STATE_CONNECTING)
        return;
    if (err != 0)
    {
        CString msg;
        msg.Format(_T("호스트(%s)에 연결할 수 없습니다. (오류 %d)\n초대 키, 같은 네트워크에 있는지, 호스트 PC의 방화벽 설정을 확인해 주세요."),
                   (LPCTSTR)HostAddress(), err);
        Fail(msg);
        return;
    }
    Writer w(HELLO);
    w.U32(MAGIC);
    w.U16(VERSION);
    w.Bytes(m_secret, sizeof(m_secret));
    w.Str(m_myId);
    s->Queue(w.Finish());
}

void CCollabSession::OnSocketClosed(CCollabSocket* s)
{
    if (s->m_dead)
        return;
    if (m_role == ROLE_CLIENT && s == m_server)
    {
        if (m_state == STATE_ACTIVE)
            Fail(_T("호스트와의 연결이 끊어졌습니다. 호스트가 공유를 끝냈거나 네트워크에 문제가 있습니다."));
        else
            Fail(_T("호스트가 연결을 닫았습니다. 초대 키가 맞는지 확인해 주세요."));
        return;
    }
    bool authed = s->m_authed;
    CString id = s->m_id;
    COLORREF color = s->m_color;
    MarkDead(s);
    if (m_role == ROLE_HOST && authed)
    {
        RemoveUser(id);
        Writer w(USER_LEAVE);
        w.Str(id);
        Broadcast(w.Finish());
        if (m_events)
        {
            m_events->OnCollabChat(id, color, id + _T(" 님이 나갔습니다."), true);
            m_events->OnCollabStateChanged();
        }
    }
}

void CCollabSession::Reject(CCollabSocket* s, RejectCode code, const CString& message)
{
    Writer w(REJECT);
    w.U8((BYTE)code);
    w.Str(message);
    s->m_closeWhenFlushed = true;
    s->Queue(w.Finish());
}

bool CCollabSession::OnMessage(CCollabSocket* s, BYTE type, Reader& r)
{
    if (m_role == ROLE_HOST)
        return HostHandle(s, type, r);
    if (m_role == ROLE_CLIENT && s == m_server)
        return ClientHandle(type, r);
    return true;
}

// ---------------------------------------------------------------------------
// 호스트 쪽 처리

bool CCollabSession::HostHandle(CCollabSocket* s, BYTE type, Reader& r)
{
    if (!s->m_authed)
    {
        if (type != HELLO)
            return false;
        DWORD magic = r.U32();
        WORD version = r.U16();
        const BYTE* secret = r.Bytes(8);
        CString id = r.Str();
        if (!r.Ok() || magic != MAGIC)
            return false;
        if (version != VERSION)
        {
            Reject(s, REJECT_VERSION, _T("호스트와 그림판 버전이 달라 참가할 수 없습니다."));
            return true;
        }
        BYTE diff = 0;
        for (int i = 0; i < 8; ++i)
            diff |= (BYTE)(secret[i] ^ m_secret[i]);
        if (diff != 0)
        {
            Reject(s, REJECT_BAD_KEY, _T("초대 키가 맞지 않습니다. 호스트에게 새 초대 키를 받아 주세요."));
            return true;
        }
        if (!IsValidUserId(id))
        {
            Reject(s, REJECT_BAD_ID, _T("사용할 수 없는 아이디입니다."));
            return true;
        }
        if (FindUser(id))
        {
            CString msg;
            msg.Format(_T("'%s' 아이디로 이미 참가한 사용자가 있습니다.\n다른 아이디로 다시 참가해 주세요."), (LPCTSTR)id);
            Reject(s, REJECT_DUPLICATE_ID, msg);
            if (m_events)
                m_events->OnCollabChat(id, RGB(128, 128, 128),
                                       _T("같은 아이디 '") + id + _T("'(으)로 들어오려는 참가 요청을 거부했습니다."), true);
            return true;
        }
        if ((int)m_users.size() >= MAX_PARTICIPANTS)
        {
            Reject(s, REJECT_FULL, _T("참가 인원이 가득 찼습니다."));
            return true;
        }

        // 아직 보내지 않은 내 편집을 기존 참가자에게 먼저 보낸 뒤 새 참가자를 받는다
        if (m_events)
            m_events->CollabFlushPending();

        bool used[USER_COLOR_COUNT] = {};
        for (auto& u : m_users)
            used[u.colorIndex % USER_COLOR_COUNT] = true;
        int ci = 0;
        while (ci < USER_COLOR_COUNT && used[ci]) ++ci;
        if (ci == USER_COLOR_COUNT) ci = (int)m_users.size();

        CollabUser nu;
        nu.id = id;
        nu.colorIndex = ci;
        nu.color = UserColor(ci);
        m_users.push_back(nu);
        s->m_authed = true;
        s->m_id = id;
        s->m_color = nu.color;

        const CBitmap32& img = m_events->CollabCurrentImage();
        std::vector<BYTE> data;
        ::GdiFlush();
        CompressPixels(img.Row(0), img.Width(), img.Height(), img.Width(), data);
        Writer w(WELCOME);
        w.U32(nu.color);
        w.U16((WORD)m_users.size());
        for (auto& u : m_users)
        {
            w.Str(u.id);
            w.U32(u.color);
            w.U8(u.isHost ? 1 : 0);
        }
        w.I32(img.Width());
        w.I32(img.Height());
        w.U32((DWORD)data.size());
        w.Bytes(data.data(), data.size());
        s->Queue(w.Finish());

        Writer j(USER_JOIN);
        j.Str(id);
        j.U32(nu.color);
        Broadcast(j.Finish(), s);
        if (m_events)
        {
            m_events->OnCollabChat(id, nu.color, id + _T(" 님이 참가했습니다."), true);
            m_events->OnCollabStateChanged();
        }
        return true;
    }

    switch (type)
    {
    case EDIT:
    {
        r.Str();
        int x = r.I32(), y = r.I32(), w = r.I32(), h = r.I32();
        DWORD len = r.U32();
        const BYTE* data = r.Bytes(len);
        if (!r.Ok()) return false;
        std::vector<DWORD> px;
        if (!DecompressPixels(data, len, w, h, px)) return false;
        if (m_events)
            m_events->OnCollabEdit(CRect(x, y, x + w, y + h), px);
        // 보낸 사람을 포함한 모두에게 호스트의 순서대로 다시 보낸다
        Writer out(EDIT);
        out.Str(s->m_id);
        out.I32(x); out.I32(y); out.I32(w); out.I32(h);
        out.U32(len);
        out.Bytes(data, len);
        Broadcast(out.Finish());
        return true;
    }
    case FULL:
    {
        r.Str();
        int w = r.I32(), h = r.I32();
        DWORD len = r.U32();
        const BYTE* data = r.Bytes(len);
        if (!r.Ok()) return false;
        std::vector<DWORD> px;
        if (!DecompressPixels(data, len, w, h, px)) return false;
        if (m_events)
            m_events->OnCollabFull(w, h, px);
        Writer out(FULL);
        out.Str(s->m_id);
        out.I32(w); out.I32(h);
        out.U32(len);
        out.Bytes(data, len);
        Broadcast(out.Finish(), s);
        return true;
    }
    case CURSOR:
    {
        r.Str();
        int x = r.I32(), y = r.I32();
        bool vis = r.U8() != 0;
        if (!r.Ok()) return false;
        if (CollabUser* u = FindUser(s->m_id))
        {
            CPoint old = u->cursor;
            bool oldVis = u->cursorVisible;
            u->cursor = CPoint(x, y);
            u->cursorVisible = vis;
            if (m_events)
                m_events->OnCollabCursor(old, oldVis, u->cursor, vis);
        }
        Writer out(CURSOR);
        out.Str(s->m_id);
        out.I32(x); out.I32(y); out.U8(vis ? 1 : 0);
        Broadcast(out.Finish(), s);
        return true;
    }
    case CHAT:
    {
        r.Str();
        CString text = r.Str();
        if (!r.Ok()) return false;
        if (m_events)
            m_events->OnCollabChat(s->m_id, s->m_color, text, false);
        Writer out(CHAT);
        out.Str(s->m_id);
        out.Str(text);
        Broadcast(out.Finish(), s);
        return true;
    }
    default:
        return true;    // 모르는 메시지는 무시
    }
}

// ---------------------------------------------------------------------------
// 참가자 쪽 처리

bool CCollabSession::ClientHandle(BYTE type, Reader& r)
{
    if (m_state == STATE_CONNECTING)
    {
        if (type == REJECT)
        {
            r.U8();
            CString msg = r.Str();
            Fail(msg.IsEmpty() ? CString(_T("호스트가 참가를 거부했습니다.")) : msg);
            return true;
        }
        if (type != WELCOME)
            return false;
        r.U32();
        WORD count = r.U16();
        std::vector<CollabUser> users;
        for (WORD i = 0; i < count; ++i)
        {
            CollabUser u;
            u.id = r.Str();
            u.color = r.U32();
            u.isHost = r.U8() != 0;
            u.isMe = (u.id.CompareNoCase(m_myId) == 0);
            users.push_back(u);
        }
        int w = r.I32(), h = r.I32();
        DWORD len = r.U32();
        const BYTE* data = r.Bytes(len);
        if (!r.Ok()) return false;
        std::vector<DWORD> px;
        if (!DecompressPixels(data, len, w, h, px)) return false;
        m_users = users;
        m_state = STATE_ACTIVE;
        if (m_events)
        {
            m_events->OnCollabFull(w, h, px);
            m_events->OnCollabJoined();
            m_events->OnCollabStateChanged();
        }
        return true;
    }

    switch (type)
    {
    case USER_JOIN:
    {
        CollabUser u;
        u.id = r.Str();
        u.color = r.U32();
        if (!r.Ok()) return false;
        if (!FindUser(u.id))
            m_users.push_back(u);
        if (m_events)
        {
            m_events->OnCollabChat(u.id, u.color, u.id + _T(" 님이 참가했습니다."), true);
            m_events->OnCollabStateChanged();
        }
        return true;
    }
    case USER_LEAVE:
    {
        CString id = r.Str();
        if (!r.Ok()) return false;
        COLORREF c = 0;
        if (CollabUser* u = FindUser(id)) c = u->color;
        RemoveUser(id);
        if (m_events)
        {
            m_events->OnCollabChat(id, c, id + _T(" 님이 나갔습니다."), true);
            m_events->OnCollabStateChanged();
        }
        return true;
    }
    case EDIT:
    {
        r.Str();
        int x = r.I32(), y = r.I32(), w = r.I32(), h = r.I32();
        DWORD len = r.U32();
        const BYTE* data = r.Bytes(len);
        if (!r.Ok()) return false;
        std::vector<DWORD> px;
        if (DecompressPixels(data, len, w, h, px) && m_events)
            m_events->OnCollabEdit(CRect(x, y, x + w, y + h), px);
        return true;
    }
    case FULL:
    {
        r.Str();
        int w = r.I32(), h = r.I32();
        DWORD len = r.U32();
        const BYTE* data = r.Bytes(len);
        if (!r.Ok()) return false;
        std::vector<DWORD> px;
        if (DecompressPixels(data, len, w, h, px) && m_events)
            m_events->OnCollabFull(w, h, px);
        return true;
    }
    case CURSOR:
    {
        CString id = r.Str();
        int x = r.I32(), y = r.I32();
        bool vis = r.U8() != 0;
        if (!r.Ok()) return false;
        if (CollabUser* u = FindUser(id))
        {
            if (u->isMe) return true;
            CPoint old = u->cursor;
            bool oldVis = u->cursorVisible;
            u->cursor = CPoint(x, y);
            u->cursorVisible = vis;
            if (m_events)
                m_events->OnCollabCursor(old, oldVis, u->cursor, vis);
        }
        return true;
    }
    case CHAT:
    {
        CString id = r.Str();
        CString text = r.Str();
        if (!r.Ok()) return false;
        CollabUser* u = FindUser(id);
        if (m_events)
            m_events->OnCollabChat(id, u ? u->color : RGB(128, 128, 128), text, false);
        return true;
    }
    default:
        return true;
    }
}
