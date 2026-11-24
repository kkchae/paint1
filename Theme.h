#pragma once
#include "framework.h"

// 밝은/어두운 테마 색
struct ThemeColors
{
    COLORREF panelBg, panelLine, text, textDim, disabled;
    COLORREF hover, pressed, checked, checkedBorder, separator;
    COLORREF canvasBg, shadow;
    COLORREF rulerBg, rulerTick, rulerText;
    COLORREF statusBg, statusText, statusLine;
    COLORREF accent, swatchBorder;
};

bool IsDarkMode();
void SetDarkMode(bool dark);
const ThemeColors& Theme();

// 시스템 설정(앱 모드)이 어두운 테마인지
bool SystemPrefersDark();

// 창 제목 표시줄을 어둡게/밝게
void ApplyThemeToTitleBar(HWND hwnd);

// 팝업 메뉴, 스크롤 막대 등 시스템 컨트롤을 현재 테마에 맞춘다
void ApplyThemeToAppMenus();
void ApplyThemeToControl(HWND hwnd);
