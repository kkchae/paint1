#pragma once
#include "ToolPanel.h"
#include "CanvasWnd.h"
#include "StatusBarEx.h"
#include "ChatPanel.h"
#include "Collab.h"
#include <memory>

#define WM_APP_COLLAB_CLEANUP   (WM_APP + 10)
#define WM_APP_COLLAB_FAILED    (WM_APP + 11)

class CMainFrame : public CFrameWnd, public ICollabEvents
{
public:
    CMainFrame();
    CCollabSession* Collab() const { return m_collab.get(); }

    void UpdateTitle();
    void SetStatusTexts(const CString& pos, const CString& sel, const CString& size, const CString& zoom, double zoomValue);
    void OnCanvasStateChanged();
    bool OpenFile(LPCTSTR path);

protected:
    CToolPanel m_panel;
    CPaintStatusBar m_status;
    CCanvasWnd m_canvas;
    CString m_path;
    CString m_ind[4];

    // ---- 공유 캔버스
    CChatPanel m_chat;
    std::unique_ptr<CCollabSession> m_collab;
    CString m_failMessage;
    void ShowChat(bool show);
    void CopyInviteKey();
    bool ConfirmEndSession(LPCTSTR action);

    // ICollabEvents
    void OnCollabStateChanged() override;
    void OnCollabChat(const CString& id, COLORREF color, const CString& text, bool system) override;
    void OnCollabEdit(const CRect& rc, const std::vector<DWORD>& px) override;
    void OnCollabFull(int w, int h, const std::vector<DWORD>& px) override;
    void OnCollabCursor(CPoint oldPos, bool oldVisible, CPoint pos, bool visible) override;
    void OnCollabJoined() override;
    void OnCollabFailed(const CString& message) override;
    void CollabFlushPending() override;
    const CBitmap32& CollabCurrentImage() override;
    void CollabRequestCleanup() override;

    afx_msg void OnCollabHost();
    afx_msg void OnCollabJoin();
    afx_msg void OnCollabLeave();
    afx_msg void OnCollabCopyKey();
    afx_msg void OnViewChat();
    afx_msg void OnUpdateCollab(CCmdUI* pCmdUI);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg LRESULT OnCollabCleanupMsg(WPARAM, LPARAM);
    afx_msg LRESULT OnCollabFailedMsg(WPARAM, LPARAM);

    bool PromptSave();
    bool DoSave(bool saveAs);
    CString DisplayName() const;
    void ApplyTheme();

    BOOL PreCreateWindow(CREATESTRUCT& cs) override;
    BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) override;
    BOOL PreTranslateMessage(MSG* pMsg) override;
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    bool DrawDarkMenuBar(UINT message, LPARAM lParam);
    void PaintMenuBarBottomLine();

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnSetFocus(CWnd* pOldWnd);
    afx_msg void OnClose();
    afx_msg void OnDropFiles(HDROP hDropInfo);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnFileSave();
    afx_msg void OnFileSaveAs();
    afx_msg void OnUpdateIndicator(CCmdUI* pCmdUI);
    afx_msg void OnViewDarkMode();
    afx_msg void OnUpdateViewDarkMode(CCmdUI* pCmdUI);
    DECLARE_MESSAGE_MAP()
};
