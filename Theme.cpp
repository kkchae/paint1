// Theme.cpp : 밝은/어두운 테마
#include "Theme.h"
#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

static bool g_dark = false;

static const ThemeColors kLight =
{
    RGB(249, 249, 249), RGB(222, 222, 222), RGB(28, 28, 28), RGB(110, 110, 110), RGB(165, 165, 165),
    RGB(234, 234, 234), RGB(214, 214, 214), RGB(220, 231, 246), RGB(150, 182, 228), RGB(222, 222, 222),
    RGB(236, 239, 244), RGB(214, 218, 225),
    RGB(249, 249, 249), RGB(150, 150, 150), RGB(90, 90, 90),
    RGB(243, 243, 243), RGB(40, 40, 40), RGB(222, 222, 222),
    RGB(0, 95, 184), RGB(165, 165, 165),
};

static const ThemeColors kDark =
{
    RGB(32, 32, 32), RGB(52, 52, 52), RGB(236, 236, 236), RGB(168, 168, 168), RGB(100, 100, 100),
    RGB(55, 55, 55), RGB(70, 70, 70), RGB(42, 60, 84), RGB(78, 122, 172), RGB(60, 60, 60),
    RGB(24, 24, 24), RGB(8, 8, 8),
    RGB(38, 38, 38), RGB(130, 130, 130), RGB(190, 190, 190),
    RGB(32, 32, 32), RGB(220, 220, 220), RGB(52, 52, 52),
    RGB(96, 170, 255), RGB(110, 110, 110),
};

bool IsDarkMode() { return g_dark; }
void SetDarkMode(bool dark) { g_dark = dark; }
const ThemeColors& Theme() { return g_dark ? kDark : kLight; }

bool SystemPrefersDark()
{
    DWORD value = 1, size = sizeof(value);
    if (::RegGetValue(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                      _T("AppsUseLightTheme"), RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
        return value == 0;
    return false;
}

void ApplyThemeToTitleBar(HWND hwnd)
{
    BOOL dark = g_dark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 20H1 이상: 20, 이전: 19)
    if (FAILED(::DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        ::DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
}

// uxtheme.dll의 문서화되지 않은 함수(서수)를 사용한다. 없으면 조용히 건너뛴다.
typedef int (WINAPI* PFN_SetPreferredAppMode)(int);
typedef void (WINAPI* PFN_FlushMenuThemes)();
typedef bool (WINAPI* PFN_AllowDarkModeForWindow)(HWND, bool);

static HMODULE UxTheme()
{
    static HMODULE h = ::LoadLibraryEx(_T("uxtheme.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    return h;
}

void ApplyThemeToAppMenus()
{
    HMODULE h = UxTheme();
    if (!h) return;
    auto setMode = (PFN_SetPreferredAppMode)::GetProcAddress(h, MAKEINTRESOURCEA(135));
    auto flush = (PFN_FlushMenuThemes)::GetProcAddress(h, MAKEINTRESOURCEA(136));
    if (setMode)
        setMode(g_dark ? 2 /*ForceDark*/ : 3 /*ForceLight*/);
    if (flush)
        flush();
}

void ApplyThemeToControl(HWND hwnd)
{
    HMODULE h = UxTheme();
    if (h)
    {
        auto allow = (PFN_AllowDarkModeForWindow)::GetProcAddress(h, MAKEINTRESOURCEA(133));
        if (allow)
            allow(hwnd, g_dark);
    }
    ::SetWindowTheme(hwnd, g_dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
}
