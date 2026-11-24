#pragma once
#include "framework.h"
#include "resource.h"

// 크기 조정 및 기울이기 대화 상자
class CResizeDlg : public CDialog
{
public:
    CResizeDlg(int w, int h, CWnd* pParent = nullptr);
    enum { IDD = IDD_RESIZE };

    int m_origW, m_origH;
    int m_newW, m_newH;
    double m_skewH = 0, m_skewV = 0;

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    afx_msg void OnModeChange();
    afx_msg void OnChangeH();
    afx_msg void OnChangeV();
    DECLARE_MESSAGE_MAP()

private:
    bool m_percent = true;
    bool m_updating = false;
};

// 공유 시작(호스트) 대화 상자
class CCollabHostDlg : public CDialog
{
public:
    explicit CCollabHostDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_COLLAB_HOST };
    CString m_id;
    DWORD m_ip = 0;
    UINT m_port = 0;

protected:
    std::vector<std::pair<DWORD, CString>> m_addrs;
    BOOL OnInitDialog() override;
    void OnOK() override;
};

// 공유 캔버스 참가 대화 상자
class CCollabJoinDlg : public CDialog
{
public:
    explicit CCollabJoinDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_COLLAB_JOIN };
    CString m_key;
    CString m_id;

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
};

CString DefaultCollabUserId();

// 이미지 속성(캔버스 크기) 대화 상자
class CCanvasSizeDlg : public CDialog
{
public:
    CCanvasSizeDlg(int w, int h, CWnd* pParent = nullptr);
    enum { IDD = IDD_CANVASSIZE };
    int m_w, m_h;

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
};
