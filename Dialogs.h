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
