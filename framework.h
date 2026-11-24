#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#include <sdkddkver.h>

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _AFX_ALL_WARNINGS

#include <afxwin.h>     // MFC 핵심 구성 요소
#include <afxext.h>     // CStatusBar 등
#include <afxdlgs.h>    // CFileDialog, CColorDialog, CFontDialog
#include <afxcmn.h>     // 공용 컨트롤
#include <afxpriv.h>    // WM_SIZEPARENT
#include <atlimage.h>   // CImage (PNG/JPEG/BMP/GIF/TIFF 입출력)
#include <gdiplus.h>

#include <vector>
#include <functional>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

template <typename T>
inline T ClampV(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
