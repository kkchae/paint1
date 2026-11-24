#pragma once
#include "Bitmap32.h"
#include "Shapes.h"

enum PaintTool
{
    TOOL_SELECT_RECT, TOOL_SELECT_FREE, TOOL_PENCIL, TOOL_FILL, TOOL_TEXT,
    TOOL_ERASER, TOOL_PICKER, TOOL_MAGNIFIER, TOOL_BRUSH, TOOL_SHAPE
};

// Windows 11 그림판의 브러시 순서
enum BrushKind
{
    BRUSH_NORMAL, BRUSH_CALLI1, BRUSH_CALLI2, BRUSH_AIR, BRUSH_OIL,
    BRUSH_CRAYON, BRUSH_MARKER, BRUSH_NATURAL_PENCIL, BRUSH_WATERCOLOR,
    BRUSH_COUNT
};

// 굵기를 따로 기억하는 도구 분류
enum SizeCategory { SC_PENCIL, SC_BRUSH, SC_ERASER, SC_SHAPE, SC_COUNT };

extern const TCHAR* g_brushNames[BRUSH_COUNT];
extern const int g_fontSizes[];
const int FONT_SIZE_COUNT = 14;
const int MIN_TOOL_SIZE = 1;
const int MAX_TOOL_SIZE = 100;
const int CUSTOM_COLOR_COUNT = 10;
const int DEFAULT_WIDTH = 1000;
const int DEFAULT_HEIGHT = 640;

class CMainFrame;

class CCanvasWnd : public CWnd
{
public:
    CCanvasWnd();
    virtual ~CCanvasWnd();
    BOOL Create(CWnd* pParent, UINT nID);

    // ---- 패널에서 조회하는 상태
    PaintTool GetTool() const { return m_tool; }
    PaintTool GetSelectMode() const { return m_selectMode; }
    ShapeType GetShape() const { return m_shape; }
    BrushKind GetBrush() const { return m_brush; }
    int GetToolSize() const { return m_toolSize[SizeCategoryOf(m_tool)]; }
    void SetToolSize(int size);
    bool IsSizeEnabled() const;
    int GetToolWidth() const { return GetToolSize(); }
    COLORREF GetColor(int slot) const { return m_color[slot]; }
    int GetActiveSlot() const { return m_activeSlot; }
    void SetActiveSlot(int slot);
    void SetColor(int slot, COLORREF c);
    void OnPaletteClick(COLORREF c, bool right);
    COLORREF GetCustomColor(int i) const { return m_custom[i]; }
    bool GetOutline() const { return m_outline; }
    bool GetFillShape() const { return m_fillShape; }
    const LOGFONT& GetTextFont() const { return m_lf; }
    int GetFontPt() const { return m_fontPt; }
    bool GetTextOpaque() const { return m_textOpaque; }
    bool CanUndo() const { return !m_undo.empty() || m_shapePending || m_curveStage > 0 || m_polyActive; }
    bool CanRedo() const { return !m_redo.empty(); }
    bool HasSelection() const { return m_selActive; }
    double GetZoom() const { return m_zoom; }
    CSize GetImageSize() const { return m_img.Size(); }
    bool GetRulers() const { return m_rulers; }

    // ---- 보기
    void SetZoomCentered(double z);
    void ZoomStepCentered(int dir);
    void ApplyTheme();

    // ---- 파일
    void NewImage(int w, int h);
    bool LoadFile(LPCTSTR path, CString& err);
    bool SaveFile(LPCTSTR path, CString& err);
    bool IsModified() const { return m_modified; }
    void SetModified(bool b);
    void FlattenPending();

    // ---- 텍스트 편집
    bool IsTextActive() const { return m_textActive; }
    bool IsTextEditFocused() const;
    void EndTextEdit(bool commit);
    void FocusDefault();

protected:
    enum DragMode
    {
        DRAG_NONE, DRAG_STROKE, DRAG_SHAPE_NEW, DRAG_SHAPE_MOVE, DRAG_SHAPE_HANDLE,
        DRAG_SEL_NEW, DRAG_SEL_FREE, DRAG_SEL_MOVE, DRAG_SEL_HANDLE, DRAG_CANVAS, DRAG_TEXT_NEW,
        DRAG_CURVE, DRAG_POLY
    };

    struct Bristle
    {
        double offset;   // 붓 중심에서의 수직 거리 (픽셀)
        double shade;    // 밝기 배율
        int width;
    };

    // ---- 이미지/실행 취소
    CBitmap32 m_img;
    std::vector<CBitmap32> m_undo, m_redo;
    bool m_modified = false;

    // ---- 설정
    PaintTool m_tool = TOOL_PENCIL;
    PaintTool m_prevTool = TOOL_PENCIL;
    PaintTool m_selectMode = TOOL_SELECT_RECT;
    ShapeType m_shape = SH_RECT;
    BrushKind m_brush = BRUSH_NORMAL;
    int m_toolSize[SC_COUNT];
    COLORREF m_color[2];
    int m_activeSlot = 0;
    bool m_outline = true;
    bool m_fillShape = false;
    COLORREF m_custom[CUSTOM_COLOR_COUNT];
    LOGFONT m_lf;
    int m_fontPt = 11;
    bool m_textOpaque = false;
    bool m_transparentSel = false;
    bool m_grid = false;
    bool m_rulers = false;

    // ---- 보기
    double m_zoom = 1.0;
    CPoint m_scroll;
    bool m_inScrollUpdate = false;
    int m_dpi = 96;
    int m_rulerSize = 22;
    CFont m_rulerFontH, m_rulerFontV;

    // ---- 마우스
    DragMode m_drag = DRAG_NONE;
    int m_dragButton = 0;
    int m_dragHandle = -1;
    CPoint m_dragStartImg, m_lastImg, m_lastMouse;
    CRect m_dragOrigRect;
    CPoint m_dragOrigP1, m_dragOrigP2;
    CSize m_canvasNewSize;
    CPoint m_hoverImg;
    bool m_hoverIn = false;
    bool m_hoverValid = false;
    bool m_trackingLeave = false;

    // ---- 도형 (확정 전)
    bool m_shapePending = false;
    CPoint m_sp1, m_sp2;
    bool m_shapeSwap = false;
    ShapeType m_pendingShape = SH_RECT;

    // ---- 곡선: 0=없음, 1=선을 그림(첫 번째 휘기 대기), 2=두 번째 휘기 대기
    int m_curveStage = 0;
    POINT m_cv[4];
    bool m_curveSwap = false;

    // ---- 다각형
    bool m_polyActive = false;
    std::vector<CPoint> m_polyPts;
    CPoint m_polyCursor;
    bool m_polySwap = false;

    // ---- 선택 영역
    bool m_selActive = false;
    bool m_selIsFree = false;
    bool m_lifted = false;
    CRect m_selRect;
    std::vector<CPoint> m_freePoly;
    CBitmap32 m_float;          // 떠 있는 선택 영역 (알파 = 마스크)
    CBitmap32 m_floatCache;     // 크기 조정된 표시용 캐시
    bool m_floatDirty = true;

    // ---- 텍스트
    CEdit m_edit;
    CFont m_editFont;
    CBrush m_editBrush;
    COLORREF m_editBrushColor = CLR_INVALID;
    bool m_textActive = false;
    CRect m_textRect;

    // ---- 붓 획
    std::vector<BYTE> m_strokeAlpha;    // 반투명 브러시: 한 획 안에서 겹쳐도 진해지지 않도록 픽셀별 최대 불투명도
    std::vector<Bristle> m_bristles;    // 유화 브러시의 붓털
    double m_oilDirX = 1.0, m_oilDirY = 0.0;

    // ---- 좌표/보기 (CanvasWnd.cpp)
    static SizeCategory SizeCategoryOf(PaintTool t);
    CMainFrame* Frame() const;
    int Z(double v) const { return (int)floor(v * m_zoom + 1e-9); }
    CRect ViewRect() const;
    CSize ZoomedSize() const;
    CPoint Origin() const;
    CPoint ScreenToImg(CPoint p) const;
    CPoint ImgToScreen(int x, int y) const;
    CRect ImgRectToScreen(const CRect& r) const;
    void UpdateScrollBars();
    void ScrollTo(int x, int y);
    void SetZoom(double z, CPoint anchor);
    void ZoomStep(int dir, CPoint anchor);
    void OnImageSizeChanged();
    void UpdateStatus();
    void NotifyState();
    void Compose(CBitmap32& out, const CRect& vis);
    void DrawOverlays(CDC& dc);
    void DrawRulers(CDC& dc);
    void InvalidateRulers();
    void GetHandleCenters(const CRect& s, CPoint pts[8]) const;
    int HitHandle(CPoint pt, const CRect& s) const;
    int HitCanvasHandle(CPoint pt) const;
    void GetCanvasHandleCenters(CPoint pts[3]) const;
    CRect ShapeScreenRect() const;
    int HitShapeHandle(CPoint pt) const;
    bool IsShapeBusy() const { return m_shapePending || m_curveStage > 0 || m_polyActive; }

    // ---- 실행 취소
    void PushUndo();
    void Undo();
    void Redo();

    // ---- 도구 (CanvasInput.cpp)
    void MouseDown(CPoint pt, int button, UINT flags);
    void MouseMove(CPoint pt, UINT flags);
    void MouseUp(CPoint pt);
    void BeginStroke();
    void EndStroke();
    void StrokeSegment(CPoint a, CPoint b);
    void DrawPenLine(CPoint a, CPoint b, int w, COLORREF c);
    void Spray(CPoint c);
    void BlendStamp(int x, int y, int alpha, COLORREF c);
    void MarkerStamp(int x, int y, int w, COLORREF c);
    void CrayonStamp(int x, int y, int w, COLORREF c);
    void WatercolorStamp(int x, int y, int w, COLORREF c);
    void NaturalPencilStamp(int x, int y, int w, COLORREF c);
    void OilSegment(CPoint a, CPoint b, int w, COLORREF c);
    bool FloodFill(CPoint p, COLORREF c);
    int ShapeWidth() const { return m_toolSize[SC_SHAPE]; }
    void DrawPendingShape(CDC* dc);
    void DrawCurve(CDC* dc);
    void DrawPolygonPreview(CDC* dc);
    void CommitShape();
    void CommitCurve();
    void CommitPolygon();
    CPoint ConstrainLine(CPoint a, CPoint b) const;
    CPoint ConstrainShape(CPoint a, CPoint b) const;
    CRect ResizeRectByHandle(const CRect& r, int handle, CPoint ip) const;

    // ---- 선택 영역
    const CBitmap32& FloatScaled();
    void BakeFloat();
    void CompositeFloat(CBitmap32& dst, int ox, int oy);
    void ApplyFreeMask(CBitmap32& b);
    void Lift();
    void CommitSelection();
    void ClearSelection();
    bool GetSelectionImage(CBitmap32& out);
    void CopySelection();
    void PasteImage();
    void DeleteSelection();
    void SelectAll();
    void CropToSelection();
    void ResizeCanvas(int w, int h);
    void ApplyTransform(std::function<CBitmap32(const CBitmap32&, bool)> fn);

    // ---- 텍스트
    static int PtToPx(int pt) { return MulDiv(pt, 96, 72); }
    void BeginTextEdit(const CRect& rc);
    void CommitText();
    void CancelText();
    void UpdateTextEditFont();
    void UpdateTextEditPos();

    // ---- 명령 (CanvasCmds.cpp)
    void SetTool(PaintTool t);
    void AddCustomColor(COLORREF c);

    // ---- 메시지
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint pt);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint pt);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint pt);
    afx_msg void OnMouseMove(UINT nFlags, CPoint pt);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnCaptureChanged(CWnd* pWnd);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnTextChange();

    afx_msg void OnToolCmd(UINT id);
    afx_msg void OnUpdateToolCmd(CCmdUI* pCmdUI);
    afx_msg void OnShapeCmd(UINT id);
    afx_msg void OnUpdateShapeCmd(CCmdUI* pCmdUI);
    afx_msg void OnBrushCmd(UINT id);
    afx_msg void OnUpdateBrushCmd(CCmdUI* pCmdUI);
    afx_msg void OnSizeCmd(UINT id);
    afx_msg void OnUpdateSizeCmd(CCmdUI* pCmdUI);
    afx_msg void OnShapeModeCmd(UINT id);
    afx_msg void OnUpdateShapeModeCmd(CCmdUI* pCmdUI);
    afx_msg void OnEditUndo();
    afx_msg void OnEditRedo();
    afx_msg void OnEditCut();
    afx_msg void OnEditCopy();
    afx_msg void OnEditPaste();
    afx_msg void OnEditSelectAll();
    afx_msg void OnEditClear();
    afx_msg void OnUpdateEditUndo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditRedo(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditCutCopy(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditPaste(CCmdUI* pCmdUI);
    afx_msg void OnUpdateEditClear(CCmdUI* pCmdUI);
    afx_msg void OnImageCmd(UINT id);
    afx_msg void OnUpdateImageCmd(CCmdUI* pCmdUI);
    afx_msg void OnSelectTransparent();
    afx_msg void OnUpdateSelectTransparent(CCmdUI* pCmdUI);
    afx_msg void OnZoomCmd(UINT id);
    afx_msg void OnViewGrid();
    afx_msg void OnUpdateViewGrid(CCmdUI* pCmdUI);
    afx_msg void OnViewRulers();
    afx_msg void OnUpdateViewRulers(CCmdUI* pCmdUI);
    afx_msg void OnColorEdit();
    afx_msg void OnColorSwap();
    afx_msg void OnTextCmd(UINT id);
    afx_msg void OnUpdateTextCmd(CCmdUI* pCmdUI);
    afx_msg void OnFontSizeCmd(UINT id);
    afx_msg void OnUpdateFontSizeCmd(CCmdUI* pCmdUI);
    DECLARE_MESSAGE_MAP()
};
