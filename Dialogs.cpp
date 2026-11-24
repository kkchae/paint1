#include "Dialogs.h"
#include "Protocol.h"

static const int MAX_DIM = 20000;

static const TCHAR kCollabSection[] = _T("Collab");

// 지난번 아이디, 없으면 Windows 사용자 이름
CString DefaultCollabUserId()
{
    CString id = AfxGetApp()->GetProfileString(kCollabSection, _T("UserId"));
    if (id.IsEmpty())
    {
        TCHAR name[64] = {};
        DWORD n = _countof(name);
        if (::GetUserName(name, &n))
            id = name;
    }
    return id.Left(Proto::MAX_ID_LENGTH);
}

static bool CheckUserId(CWnd* dlg, CString& id)
{
    id.Trim();
    if (!Proto::IsValidUserId(id))
    {
        CString msg;
        msg.Format(_T("아이디를 1~%d글자로 입력해 주세요."), Proto::MAX_ID_LENGTH);
        dlg->MessageBox(msg, _T("그림판"), MB_ICONWARNING);
        dlg->GetDlgItem(IDC_COLLAB_ID)->SetFocus();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------

CCollabHostDlg::CCollabHostDlg(CWnd* pParent) : CDialog(IDD_COLLAB_HOST, pParent)
{
}

BOOL CCollabHostDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetDlgItemText(IDC_COLLAB_ID, DefaultCollabUserId());
    ((CEdit*)GetDlgItem(IDC_COLLAB_ID))->LimitText(Proto::MAX_ID_LENGTH);

    CComboBox* combo = (CComboBox*)GetDlgItem(IDC_COLLAB_IP);
    DWORD lastIp = (DWORD)AfxGetApp()->GetProfileInt(kCollabSection, _T("HostIp"), 0);
    int sel = 0;
    for (auto& a : Proto::EnumLocalAddresses())
    {
        int i = combo->AddString(a.label);
        combo->SetItemData(i, a.ip);
        if (a.ip == lastIp)
            sel = i;
    }
    combo->SetCurSel(sel);
    SetDlgItemInt(IDC_COLLAB_PORT, AfxGetApp()->GetProfileInt(kCollabSection, _T("Port"), Proto::DEFAULT_PORT));
    return TRUE;
}

void CCollabHostDlg::OnOK()
{
    GetDlgItemText(IDC_COLLAB_ID, m_id);
    if (!CheckUserId(this, m_id))
        return;
    CComboBox* combo = (CComboBox*)GetDlgItem(IDC_COLLAB_IP);
    int sel = combo->GetCurSel();
    if (sel < 0)
    {
        MessageBox(_T("네트워크 주소를 선택해 주세요."), _T("그림판"), MB_ICONWARNING);
        return;
    }
    m_ip = (DWORD)combo->GetItemData(sel);
    BOOL ok = FALSE;
    m_port = GetDlgItemInt(IDC_COLLAB_PORT, &ok, FALSE);
    if (!ok || m_port < 1024 || m_port > 65000)
    {
        MessageBox(_T("포트는 1024~65000 사이의 숫자여야 합니다."), _T("그림판"), MB_ICONWARNING);
        return;
    }
    AfxGetApp()->WriteProfileString(kCollabSection, _T("UserId"), m_id);
    AfxGetApp()->WriteProfileInt(kCollabSection, _T("HostIp"), (int)m_ip);
    AfxGetApp()->WriteProfileInt(kCollabSection, _T("Port"), (int)m_port);
    CDialog::OnOK();
}

// ---------------------------------------------------------------------------

CCollabJoinDlg::CCollabJoinDlg(CWnd* pParent) : CDialog(IDD_COLLAB_JOIN, pParent)
{
}

BOOL CCollabJoinDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetDlgItemText(IDC_COLLAB_ID, DefaultCollabUserId());
    ((CEdit*)GetDlgItem(IDC_COLLAB_ID))->LimitText(Proto::MAX_ID_LENGTH);

    // 클립보드에 초대 키가 있으면 미리 채운다
    if (OpenClipboard())
    {
        HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
        if (h)
        {
            CString text((LPCWSTR)::GlobalLock(h));
            ::GlobalUnlock(h);
            text.Trim();
            DWORD ip; WORD port; BYTE secret[8];
            if (text.GetLength() < 64 && Proto::DecodeInviteKey(text, ip, port, secret))
                SetDlgItemText(IDC_COLLAB_KEY, text);
        }
        ::CloseClipboard();
    }
    GetDlgItem(IDC_COLLAB_KEY)->SetFocus();
    return FALSE;
}

void CCollabJoinDlg::OnOK()
{
    GetDlgItemText(IDC_COLLAB_KEY, m_key);
    m_key.Trim();
    DWORD ip; WORD port; BYTE secret[8];
    if (!Proto::DecodeInviteKey(m_key, ip, port, secret))
    {
        MessageBox(_T("초대 키가 올바르지 않습니다.\n호스트에게 받은 키(예: ABCD-EFGH-...)를 그대로 붙여넣어 주세요."),
                   _T("그림판"), MB_ICONWARNING);
        GetDlgItem(IDC_COLLAB_KEY)->SetFocus();
        return;
    }
    GetDlgItemText(IDC_COLLAB_ID, m_id);
    if (!CheckUserId(this, m_id))
        return;
    AfxGetApp()->WriteProfileString(kCollabSection, _T("UserId"), m_id);
    CDialog::OnOK();
}

CResizeDlg::CResizeDlg(int w, int h, CWnd* pParent)
    : CDialog(IDD_RESIZE, pParent), m_origW(w), m_origH(h), m_newW(w), m_newH(h)
{
}

BEGIN_MESSAGE_MAP(CResizeDlg, CDialog)
    ON_BN_CLICKED(IDC_RB_PERCENT, &CResizeDlg::OnModeChange)
    ON_BN_CLICKED(IDC_RB_PIXELS, &CResizeDlg::OnModeChange)
    ON_EN_CHANGE(IDC_EDIT_H, &CResizeDlg::OnChangeH)
    ON_EN_CHANGE(IDC_EDIT_V, &CResizeDlg::OnChangeV)
END_MESSAGE_MAP()

BOOL CResizeDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_updating = true;
    CheckRadioButton(IDC_RB_PERCENT, IDC_RB_PIXELS, IDC_RB_PERCENT);
    SetDlgItemInt(IDC_EDIT_H, 100);
    SetDlgItemInt(IDC_EDIT_V, 100);
    CheckDlgButton(IDC_CHK_ASPECT, BST_CHECKED);
    SetDlgItemText(IDC_EDIT_SKEWH, _T("0"));
    SetDlgItemText(IDC_EDIT_SKEWV, _T("0"));
    m_updating = false;
    return TRUE;
}

void CResizeDlg::OnModeChange()
{
    bool percent = IsDlgButtonChecked(IDC_RB_PERCENT) == BST_CHECKED;
    if (percent == m_percent)
        return;
    int h = (int)GetDlgItemInt(IDC_EDIT_H);
    int v = (int)GetDlgItemInt(IDC_EDIT_V);
    m_updating = true;
    if (percent)
    {
        SetDlgItemInt(IDC_EDIT_H, (UINT)max(1L, lround(h * 100.0 / m_origW)));
        SetDlgItemInt(IDC_EDIT_V, (UINT)max(1L, lround(v * 100.0 / m_origH)));
    }
    else
    {
        SetDlgItemInt(IDC_EDIT_H, (UINT)max(1L, lround(m_origW * h / 100.0)));
        SetDlgItemInt(IDC_EDIT_V, (UINT)max(1L, lround(m_origH * v / 100.0)));
    }
    m_updating = false;
    m_percent = percent;
}

void CResizeDlg::OnChangeH()
{
    if (m_updating || IsDlgButtonChecked(IDC_CHK_ASPECT) != BST_CHECKED)
        return;
    BOOL ok = FALSE;
    int h = (int)GetDlgItemInt(IDC_EDIT_H, &ok);
    if (!ok) return;
    m_updating = true;
    if (m_percent)
        SetDlgItemInt(IDC_EDIT_V, (UINT)h);
    else
        SetDlgItemInt(IDC_EDIT_V, (UINT)max(1L, lround((double)h * m_origH / m_origW)));
    m_updating = false;
}

void CResizeDlg::OnChangeV()
{
    if (m_updating || IsDlgButtonChecked(IDC_CHK_ASPECT) != BST_CHECKED)
        return;
    BOOL ok = FALSE;
    int v = (int)GetDlgItemInt(IDC_EDIT_V, &ok);
    if (!ok) return;
    m_updating = true;
    if (m_percent)
        SetDlgItemInt(IDC_EDIT_H, (UINT)v);
    else
        SetDlgItemInt(IDC_EDIT_H, (UINT)max(1L, lround((double)v * m_origW / m_origH)));
    m_updating = false;
}

void CResizeDlg::OnOK()
{
    BOOL ok1 = FALSE, ok2 = FALSE;
    int h = (int)GetDlgItemInt(IDC_EDIT_H, &ok1);
    int v = (int)GetDlgItemInt(IDC_EDIT_V, &ok2);
    if (!ok1 || !ok2 || h <= 0 || v <= 0)
    {
        AfxMessageBox(_T("올바른 크기를 입력하세요."), MB_ICONWARNING);
        return;
    }
    if (m_percent)
    {
        m_newW = max(1, (int)lround(m_origW * h / 100.0));
        m_newH = max(1, (int)lround(m_origH * v / 100.0));
    }
    else
    {
        m_newW = h;
        m_newH = v;
    }
    if (m_newW > MAX_DIM || m_newH > MAX_DIM)
    {
        AfxMessageBox(_T("이미지가 너무 큽니다. 가로와 세로는 20000픽셀 이하여야 합니다."), MB_ICONWARNING);
        return;
    }

    CString s;
    GetDlgItemText(IDC_EDIT_SKEWH, s);
    m_skewH = _tstof(s);
    GetDlgItemText(IDC_EDIT_SKEWV, s);
    m_skewV = _tstof(s);
    if (m_skewH < -89 || m_skewH > 89 || m_skewV < -89 || m_skewV > 89)
    {
        AfxMessageBox(_T("기울이기 각도는 -89도에서 89도 사이여야 합니다."), MB_ICONWARNING);
        return;
    }
    CDialog::OnOK();
}

// ---------------------------------------------------------------------------

CCanvasSizeDlg::CCanvasSizeDlg(int w, int h, CWnd* pParent)
    : CDialog(IDD_CANVASSIZE, pParent), m_w(w), m_h(h)
{
}

BOOL CCanvasSizeDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    SetDlgItemInt(IDC_EDIT_WIDTH, (UINT)m_w);
    SetDlgItemInt(IDC_EDIT_HEIGHT, (UINT)m_h);
    CString s;
    s.Format(_T("현재 크기: %d × %d 픽셀"), m_w, m_h);
    SetDlgItemText(IDC_STATIC_CUR, s);
    return TRUE;
}

void CCanvasSizeDlg::OnOK()
{
    BOOL ok1 = FALSE, ok2 = FALSE;
    int w = (int)GetDlgItemInt(IDC_EDIT_WIDTH, &ok1);
    int h = (int)GetDlgItemInt(IDC_EDIT_HEIGHT, &ok2);
    if (!ok1 || !ok2 || w <= 0 || h <= 0 || w > MAX_DIM || h > MAX_DIM)
    {
        AfxMessageBox(_T("가로와 세로는 1에서 20000 사이의 값이어야 합니다."), MB_ICONWARNING);
        return;
    }
    m_w = w;
    m_h = h;
    CDialog::OnOK();
}
