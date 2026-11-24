#include "Dialogs.h"

static const int MAX_DIM = 20000;

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
