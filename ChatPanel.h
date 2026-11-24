#pragma once
#include "framework.h"
#include <afxcmn.h>

class CCollabSession;

// 화면 오른쪽의 공유 상태 / 참가자 / 채팅 패널
class CChatPanel : public CWnd
{
public:
    BOOL Create(CWnd* pParent, CCollabSession* session);
    void Refresh();
    void AddMessage(const CString& who, COLORREF color, const CString& text, bool system);
    void ApplyTheme();
    int PanelWidth() const { return S(300); }

    std::function<void(const CString&)> OnSend;
    std::function<void()> OnCopyKey;
    std::function<void()> OnHide;

protected:
    struct ChatLine
    {
        CString who;
        COLORREF color;
        CString text;
        bool system;
        CString time;
    };
    enum HotButton { HOT_NONE, HOT_COPY, HOT_SEND, HOT_CLOSE };

    CCollabSession* m_session = nullptr;
    CRichEditCtrl m_log;
    CEdit m_input;
    CFont m_font, m_bold, m_title, m_mono;
    CBrush m_inputBrush;
    COLORREF m_inputBrushColor = CLR_INVALID;
    std::vector<ChatLine> m_lines;
    int m_dpi = 96;
    CRect m_copyBtn, m_sendBtn, m_closeBtn, m_keyBox, m_inputBox;
    int m_usersTop = 0, m_chatTop = 0;
    HotButton m_hot = HOT_NONE, m_pressed = HOT_NONE;
    bool m_tracking = false;
    DWORD m_copiedAt = 0;

    int S(int v) const { return MulDiv(v, m_dpi, 96); }
    void Layout();
    void AppendLine(const ChatLine& line);
    void RenderLog();
    void DoSend();
    HotButton HitButton(CPoint pt) const;
    COLORREF ReadableColor(COLORREF c) const;
    void DrawButton(CDC& dc, const CRect& r, LPCTSTR text, bool hot, bool pressed, bool primary);

    BOOL PreTranslateMessage(MSG* pMsg) override;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnMouseLeave();
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnInputFocus();
    afx_msg LRESULT OnSizeParent(WPARAM wParam, LPARAM lParam);
    DECLARE_MESSAGE_MAP()
};
