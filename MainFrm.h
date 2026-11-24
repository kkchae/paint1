#pragma once
#include "ToolPanel.h"
#include "CanvasWnd.h"
#include "StatusBarEx.h"

class CMainFrame : public CFrameWnd
{
public:
    CMainFrame();

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
