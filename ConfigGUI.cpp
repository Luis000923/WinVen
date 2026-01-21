#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include "ConfigGUI.h"
#include "WindowManager.h"
#include <algorithm>
#include <commctrl.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <iostream>
#include <shlobj.h>
#include <string>
#include <tlhelp32.h>
#include <vector>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "uxtheme.lib")

using namespace Gdiplus;

// IDs
#define ID_BTN_SAVE 2001
#define ID_SEARCH_BOX 2002
#define ID_APP_LISTBOX 3001
#define ID_BTN_SELECT_APP 3002
#define MARGIN_SIDEBAR 220

extern HINSTANCE hAppInstance; // Defined in gestor_ven.cpp or we pass it

// Internal Globals
static WindowManager *guiManager = nullptr;
static HINSTANCE guiInstance = nullptr;
static int currentTab = 0;
static int scrollOffset = 0;
static int maxScroll = 0;
static int hoveredCardIndex = -1;
static DWORD mainThreadId = 0; // Almacena el ID del hilo principal
static bool isDraggingMargin = false;
static bool isDraggingTransparency = false;
static RECT marginSliderRect = {0};
static RECT transparencySliderRect = {0};

// Temporary UI state
static bool tempSounds = true;
static bool tempAnimations = true;
static bool tempTray = true;
static bool tempLogging = false;
static int tempMargin = 6;
static int tempTransparency = 180;

struct AppCard {
  RectF rect;
  int index;
  std::wstring name;
  std::wstring path;
  wchar_t key;
  bool isHovered;
};

static std::vector<AppCard> cards;
static std::vector<DiscoveryApp> discoveredApps;
static std::vector<DiscoveryApp> filteredApps;

// --- Helpers ---
static std::wstring ToWString(const std::string &s) {
  if (s.empty())
    return L"";
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
  std::wstring strTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &strTo[0], size_needed);
  return strTo;
}

static std::wstring ToLower(const std::wstring &s) {
  std::wstring result = s;
  for (auto &c : result)
    c = towlower(c);
  return result;
}

static bool ContainsIgnoreCase(const std::wstring &str,
                               const std::wstring &query) {
  return ToLower(str).find(ToLower(query)) != std::wstring::npos;
}

static bool IsAutoStartEnabled() {
  if (guiManager)
    return guiManager->IsAutoStartEnabled();
  return false;
}

static void SetAutoStart(bool enable) {
  if (guiManager) {
    guiManager->SetAutoStartEnabled(enable);
    guiManager->SaveConfig();
  }
}

// --- Selecting App Logic ---
static HWND hAppSelectWnd = NULL;
static HWND hSearchEdit = NULL;
static HWND hAppListBox = NULL;
static int selectedDiscoveryIdx = -1;

static void FilterAppList() {
  if (!hAppListBox)
    return;
  wchar_t searchText[256] = {0};
  if (hSearchEdit)
    GetWindowTextW(hSearchEdit, searchText, 256);
  std::wstring query = searchText;

  SendMessageW(hAppListBox, LB_RESETCONTENT, 0, 0);
  filteredApps.clear();

  std::vector<DiscoveryApp> sortedApps = discoveredApps;
  std::sort(sortedApps.begin(), sortedApps.end(),
            [](const DiscoveryApp &a, const DiscoveryApp &b) {
              return ToLower(ToWString(a.name)) < ToLower(ToWString(b.name));
            });

  for (const auto &app : sortedApps) {
    std::wstring wName = ToWString(app.name);
    if (query.empty() || ContainsIgnoreCase(wName, query)) {
      filteredApps.push_back(app);
      SendMessageW(hAppListBox, LB_ADDSTRING, 0, (LPARAM)wName.c_str());
    }
  }
}

static LRESULT CALLBACK AppSelectProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                      LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE: {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    CreateWindowExW(0, L"STATIC", L"Buscar aplicacion:", WS_CHILD | WS_VISIBLE,
                    20, 15, 200, 25, hwnd, NULL, guiInstance, NULL);
    hSearchEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        20, 45, 360, 30, hwnd, (HMENU)ID_SEARCH_BOX, guiInstance, NULL);
    hAppListBox = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 20, 85, 360, 350, hwnd,
        (HMENU)ID_APP_LISTBOX, guiInstance, NULL);
    CreateWindowExW(0, L"BUTTON", L"Seleccionar",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 40, 445, 140, 40,
                    hwnd, (HMENU)ID_BTN_SELECT_APP, guiInstance, NULL);
    CreateWindowExW(0, L"BUTTON", L"Examinar...",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 220, 445, 140, 40,
                    hwnd, (HMENU)3003, guiInstance, NULL);

    // Font logic omitted for brevity, using system default is acceptable or we
    // re-create
    HFONT hFont =
        CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    SendMessage(GetDlgItem(hwnd, ID_SEARCH_BOX), WM_SETFONT, (WPARAM)hFont,
                TRUE);
    SendMessage(GetDlgItem(hwnd, ID_APP_LISTBOX), WM_SETFONT, (WPARAM)hFont,
                TRUE);
    SendMessage(GetDlgItem(hwnd, ID_BTN_SELECT_APP), WM_SETFONT, (WPARAM)hFont,
                TRUE);
    SendMessage(GetDlgItem(hwnd, 3003), WM_SETFONT, (WPARAM)hFont, TRUE);

    if (guiManager)
      discoveredApps = guiManager->DiscoverSystemApps();
    FilterAppList();
    SetFocus(hSearchEdit);
    return 0;
  }
  case WM_COMMAND: {
    if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == ID_SEARCH_BOX)
      FilterAppList();
    if (LOWORD(wParam) == ID_BTN_SELECT_APP ||
        (HIWORD(wParam) == LBN_DBLCLK && LOWORD(wParam) == ID_APP_LISTBOX)) {
      int idx = (int)SendMessageW(hAppListBox, LB_GETCURSEL, 0, 0);
      if (idx != LB_ERR && idx < (int)filteredApps.size()) {
        for (int i = 0; i < (int)discoveredApps.size(); i++) {
          if (discoveredApps[i].path == filteredApps[idx].path) {
            selectedDiscoveryIdx = i;
            break;
          }
        }
        DestroyWindow(hwnd);
      }
    }
    if (LOWORD(wParam) == 3003) { // Browse
      OPENFILENAMEW ofn;
      wchar_t szFile[MAX_PATH] = {0};
      ZeroMemory(&ofn, sizeof(ofn));
      ofn.lStructSize = sizeof(ofn);
      ofn.hwndOwner = hwnd;
      ofn.lpstrFile = szFile;
      ofn.nMaxFile = sizeof(szFile);
      ofn.lpstrFilter = L"Executables (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
      ofn.nFilterIndex = 1;
      ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
      if (GetOpenFileNameW(&ofn) == TRUE) {
        // Quick convert
        char buf[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, buf, MAX_PATH, NULL,
                            NULL);
        std::string pathUTF8 = buf;
        std::string name = pathUTF8.substr(pathUTF8.find_last_of("\\/") + 1);
        size_t dot = name.find_last_of(".");
        if (dot != std::string::npos)
          name = name.substr(0, dot);

        DiscoveryApp newApp;
        newApp.name = name;
        newApp.path = pathUTF8;
        discoveredApps.push_back(newApp);
        selectedDiscoveryIdx = (int)discoveredApps.size() - 1;
        DestroyWindow(hwnd);
      }
    }
    return 0;
  }
  case WM_CTLCOLORSTATIC:
  case WM_CTLCOLOREDIT:
  case WM_CTLCOLORLISTBOX: {
    HDC hdcStatic = (HDC)wParam;
    SetTextColor(hdcStatic, RGB(240, 240, 245));
    SetBkColor(hdcStatic, RGB(35, 35, 45));
    static HBRUSH hBrush = CreateSolidBrush(RGB(35, 35, 45));
    return (LRESULT)hBrush;
  }
  case WM_CLOSE:
    hSearchEdit = NULL;
    hAppListBox = NULL;
    DestroyWindow(hwnd);
    return 0;
  }
  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- Key Prompt ---
static wchar_t capturedKey = 0;
static LRESULT CALLBACK KeyPromptProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                      LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE: {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 35));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(240, 240, 250));
    DrawTextW(hdc, L"Presiona una letra (A-Z)", -1, &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_CHAR: {
    wchar_t c = towupper((wchar_t)wParam);
    if (c >= L'A' && c <= L'Z') {
      capturedKey = c;
      DestroyWindow(hwnd);
    }
    return 0;
  }
  }
  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- Painting ---
void PaintAppsTab(Graphics &g, FontFamily *fontFamily, REAL contentX,
                  REAL contentY, REAL contentW, REAL contentH) {
  SolidBrush whiteBrush(Color(255, 245, 245, 250));
  SolidBrush dimBrush(Color(255, 140, 140, 160));
  Font fontSection(fontFamily, 14, FontStyleBold, UnitPixel);
  g.DrawString(L"ACCESOS DIRECTOS", -1, &fontSection,
               PointF(contentX, contentY), &dimBrush);

  cards.clear();
  if (!guiManager)
    return;
  auto shortcuts = guiManager->GetAppShortcuts();

  REAL startY = contentY + 40;
  REAL cardW = 280.0f;
  REAL cardH = 90.0f;
  REAL gutter = 20.0f;
  int cols = 2;
  int totalHeight = 0;

  int i = 0;
  for (; i < (int)shortcuts.size(); ++i) {
    int col = i % cols;
    int row = i / cols;
    REAL x = contentX + (col * (cardW + gutter));
    REAL y = startY + (row * (cardH + gutter)) - scrollOffset;
    totalHeight = (row + 1) * (cardH + gutter);

    if (y > contentY + contentH)
      continue;

    RectF cardRect(x, y, cardW, cardH);
    std::wstring wName = ToWString(shortcuts[i].name);
    std::wstring wPath = ToWString(shortcuts[i].path);
    bool isHovered = (hoveredCardIndex == i);
    cards.push_back(
        {cardRect, i, wName, wPath, (wchar_t)shortcuts[i].hotkey, isHovered});

    if (y < contentY - cardH)
      continue;

    Color cardColor =
        isHovered ? Color(255, 38, 38, 48) : Color(255, 28, 28, 36);
    SolidBrush cardBrush(cardColor);
    g.FillRectangle(&cardBrush, cardRect);
    Pen borderPen(Color(80, 255, 255, 255), 1.0f);
    g.DrawRectangle(&borderPen, cardRect);
    SolidBrush accentBrush(Color(255, 99, 102, 241));
    g.FillRectangle(&accentBrush, x, y, 4.0f, cardH);

    Font fontAppName(fontFamily, 16, FontStyleBold, UnitPixel);
    RectF nameRect(x + 20, y + 15, cardW - 100, 25);
    StringFormat sf;
    sf.SetTrimming(StringTrimmingEllipsisCharacter);
    g.DrawString(wName.c_str(), -1, &fontAppName, nameRect, &sf, &whiteBrush);
    Font fontPath(fontFamily, 11, FontStyleRegular, UnitPixel);
    RectF pathRect(x + 20, y + 42, cardW - 30, 18);
    g.DrawString(wPath.c_str(), -1, &fontPath, pathRect, &sf, &dimBrush);

    RectF badgeRect(x + cardW - 85, y + 12, 70, 28);
    LinearGradientBrush badgeGrad(badgeRect, Color(255, 99, 102, 241),
                                  Color(255, 139, 92, 246),
                                  LinearGradientModeHorizontal);
    g.FillRectangle(&badgeGrad, badgeRect);
    std::wstring keyLabel = L"Alt+";
    keyLabel += (wchar_t)shortcuts[i].hotkey;
    Font fontBadge(fontFamily, 12, FontStyleBold, UnitPixel);
    StringFormat badgeFormat;
    badgeFormat.SetAlignment(StringAlignmentCenter);
    badgeFormat.SetLineAlignment(StringAlignmentCenter);
    SolidBrush badgeText(Color(255, 255, 255, 255));
    g.DrawString(keyLabel.c_str(), -1, &fontBadge, badgeRect, &badgeFormat,
                 &badgeText);

    if (isHovered) {
      Font fontX(fontFamily, 14, FontStyleBold, UnitPixel);
      RectF xRect(x + cardW - 25, y + cardH - 25, 20, 20);
      SolidBrush xBrush(Color(200, 239, 68, 68));
      g.DrawString(L"X", -1, &fontX, xRect, NULL, &xBrush);
    }
  }

  // Plus button
  int col = i % cols;
  int row = i / cols;
  REAL x = contentX + (col * (cardW + gutter));
  REAL y = startY + (row * (cardH + gutter)) - scrollOffset;
  totalHeight = (row + 1) * (cardH + gutter) + 100;

  RectF plusRect(x, y, cardW, cardH);
  cards.push_back({plusRect, -1, L"", L"", 0, false});
  if (y >= contentY - cardH && y <= contentY + contentH) {
    Pen plusDash(Color(100, 99, 102, 241), 2.0f);
    plusDash.SetDashStyle(DashStyleDash);
    g.DrawRectangle(&plusDash, plusRect);
    Font fontPlus(fontFamily, 36, FontStyleRegular, UnitPixel);
    StringFormat formatPlus;
    formatPlus.SetAlignment(StringAlignmentCenter);
    formatPlus.SetLineAlignment(StringAlignmentCenter);
    SolidBrush plusBrush(Color(255, 99, 102, 241));
    g.DrawString(L"+", -1, &fontPlus, plusRect, &formatPlus, &plusBrush);
  }
  maxScroll = (int)(totalHeight - contentH);
  if (maxScroll < 0)
    maxScroll = 0;
}

void PaintSettingsTab(Graphics &g, FontFamily *fontFamily, REAL contentX,
                      REAL contentY, REAL contentW, REAL contentH) {
  SolidBrush whiteBrush(Color(255, 245, 245, 250));
  SolidBrush dimBrush(Color(255, 140, 140, 160));
  SolidBrush accentBrush(Color(255, 99, 102, 241));
  SolidBrush cardBrush(Color(255, 28, 28, 36));
  SolidBrush circleBrush(Color(255, 255, 255, 255));
  SolidBrush trackBrush(Color(255, 50, 50, 60));
  Pen borderPen(Color(60, 255, 255, 255), 1.0f);

  REAL startY = contentY;
  REAL y = startY - scrollOffset;
  REAL sectionW = contentW - 40;

  Font fontSection(fontFamily, 12, FontStyleBold, UnitPixel);
  Font fontOption(fontFamily, 15, FontStyleBold, UnitPixel);
  Font fontDesc(fontFamily, 11, FontStyleRegular, UnitPixel);

  g.DrawString(L"GENERAL", -1, &fontSection, PointF(contentX, y), &dimBrush);
  y += 30;

  // AutoStart
  RectF cardAuto(contentX, y, sectionW, 75);
  g.FillRectangle(&cardBrush, cardAuto);
  g.DrawRectangle(&borderPen, cardAuto);
  g.DrawString(L"Iniciar con Windows", -1, &fontOption,
               PointF(contentX + 20, y + 15), &whiteBrush);
  g.DrawString(L"WinVen se ejecutara automaticamente al encender el PC", -1,
               &fontDesc, PointF(contentX + 20, y + 42), &dimBrush);
  RectF toggleRect(contentX + sectionW - 60, y + 25, 44, 24);
  bool autoStartOn = IsAutoStartEnabled();
  Color autoStartCol =
      autoStartOn ? Color(255, 34, 197, 94) : Color(255, 70, 70, 80);
  SolidBrush autoStartBrush(autoStartCol);
  g.FillRectangle(&autoStartBrush, toggleRect);
  g.FillEllipse(&circleBrush,
                (REAL)(autoStartOn ? toggleRect.X + 22 : toggleRect.X + 2),
                toggleRect.Y + 2, 20.0f, 20.0f);
  y += 90;

  // Tray
  RectF cardTray(contentX, y, sectionW, 75);
  g.FillRectangle(&cardBrush, cardTray);
  g.DrawRectangle(&borderPen, cardTray);
  g.DrawString(L"Icono en bandeja del sistema", -1, &fontOption,
               PointF(contentX + 20, y + 15), &whiteBrush);
  g.DrawString(L"Muestra un acceso rapido en la barra de tareas", -1, &fontDesc,
               PointF(contentX + 20, y + 42), &dimBrush);
  RectF toggleTray(contentX + sectionW - 60, y + 25, 44, 24);
  Color trayBg = tempTray ? Color(255, 34, 197, 94) : Color(255, 70, 70, 80);
  SolidBrush trayBrush(trayBg);
  g.FillRectangle(&trayBrush, toggleTray);
  g.FillEllipse(&circleBrush,
                (REAL)(tempTray ? toggleTray.X + 22 : toggleTray.X + 2),
                toggleTray.Y + 2, 20.0f, 20.0f);
  y += 110;

  // Perf
  g.DrawString(L"RENDIMIENTO", -1, &fontSection, PointF(contentX, y),
               &dimBrush);
  y += 30;

  RectF cardAnim(contentX, y, sectionW, 75);
  g.FillRectangle(&cardBrush, cardAnim);
  g.DrawRectangle(&borderPen, cardAnim);
  g.DrawString(L"Animaciones suaves", -1, &fontOption,
               PointF(contentX + 20, y + 15), &whiteBrush);
  g.DrawString(L"Transiciones fluidas al mover o redimensionar ventanas", -1,
               &fontDesc, PointF(contentX + 20, y + 42), &dimBrush);
  RectF toggleAnim(contentX + sectionW - 60, y + 25, 44, 24);
  Color animBg =
      tempAnimations ? Color(255, 34, 197, 94) : Color(255, 70, 70, 80);
  SolidBrush animBrush(animBg);
  g.FillRectangle(&animBrush, toggleAnim);
  g.FillEllipse(&circleBrush,
                (REAL)(tempAnimations ? toggleAnim.X + 22 : toggleAnim.X + 2),
                toggleAnim.Y + 2, 20.0f, 20.0f);
  y += 90;

  RectF cardSound(contentX, y, sectionW, 75);
  g.FillRectangle(&cardBrush, cardSound);
  g.DrawRectangle(&borderPen, cardSound);
  g.DrawString(L"Sonidos de feedback", -1, &fontOption,
               PointF(contentX + 20, y + 15), &whiteBrush);
  g.DrawString(L"Efectos sonoros al activar atajos o funciones", -1, &fontDesc,
               PointF(contentX + 20, y + 42), &dimBrush);
  RectF toggleSound(contentX + sectionW - 60, y + 25, 44, 24);
  Color soundBg = tempSounds ? Color(255, 34, 197, 94) : Color(255, 70, 70, 80);
  SolidBrush soundBrush(soundBg);
  g.FillRectangle(&soundBrush, toggleSound);
  g.FillEllipse(&circleBrush,
                (REAL)(tempSounds ? toggleSound.X + 22 : toggleSound.X + 2),
                toggleSound.Y + 2, 20.0f, 20.0f);
  y += 110;

  // Appearance
  g.DrawString(L"APARIENCIA", -1, &fontSection, PointF(contentX, y), &dimBrush);
  y += 30;

  // Margin
  RectF cardMargin(contentX, y, sectionW, 100);
  g.FillRectangle(&cardBrush, cardMargin);
  g.DrawRectangle(&borderPen, cardMargin);
  g.DrawString(L"Margen entre ventanas", -1, &fontOption,
               PointF(contentX + 20, y + 15), &whiteBrush);
  wchar_t marginText[128];
  swprintf(marginText, 128,
           L"Espaciado: %d px - Pixeles de separacion en layouts", tempMargin);
  g.DrawString(marginText, -1, &fontDesc, PointF(contentX + 20, y + 42),
               &dimBrush);
  RectF sliderTrack(contentX + 20, y + 70, sectionW - 40, 8);
  g.FillRectangle(&trackBrush, sliderTrack);
  REAL sliderProgress = (REAL)(tempMargin) / 20.0f;
  g.FillRectangle(&accentBrush, sliderTrack.X, sliderTrack.Y,
                  sliderTrack.Width * sliderProgress, 8.0f);
  g.FillEllipse(&circleBrush,
                sliderTrack.X + (sliderTrack.Width * sliderProgress) - 8,
                y + 66, 16.0f, 16.0f);
  y += 120;

  // Transp
  RectF cardTrans(contentX, y, sectionW, 100);
  g.FillRectangle(&cardBrush, cardTrans);
  g.DrawRectangle(&borderPen, cardTrans);
  g.DrawString(L"Transparencia", -1, &fontOption, PointF(contentX + 20, y + 15),
               &whiteBrush);
  wchar_t transText[128];
  swprintf(transText, 128,
           L"Nivel: %d - Opacidad aplicada a ventanas (Ctrl+Shift+T)",
           tempTransparency);
  g.DrawString(transText, -1, &fontDesc, PointF(contentX + 20, y + 42),
               &dimBrush);
  RectF sliderTransTrack(contentX + 20, y + 70, sectionW - 40, 8);
  g.FillRectangle(&trackBrush, sliderTransTrack);
  REAL transProgress = (REAL)(tempTransparency) / 255.0f;
  g.FillRectangle(&accentBrush, sliderTransTrack.X, sliderTransTrack.Y,
                  sliderTransTrack.Width * transProgress, 8.0f);
  g.FillEllipse(&circleBrush,
                sliderTransTrack.X + (sliderTransTrack.Width * transProgress) -
                    8,
                y + 66, 16.0f, 16.0f);
  y += 125;

  // Logs (Avanzado)
  g.DrawString(L"AVANZADO", -1, &fontSection, PointF(contentX, y), &dimBrush);
  y += 30;
  RectF cardLog(contentX, y, sectionW, 75);
  g.FillRectangle(&cardBrush, cardLog);
  g.DrawRectangle(&borderPen, cardLog);
  g.DrawString(L"Registro de actividad (Logger)", -1, &fontOption,
               PointF(contentX + 20, y + 15), &whiteBrush);
  g.DrawString(L"Guarda acciones detalladas en winven.log para diagnostico", -1,
               &fontDesc, PointF(contentX + 20, y + 42), &dimBrush);
  RectF toggleLog(contentX + sectionW - 60, y + 25, 44, 24);
  Color logBg = tempLogging ? Color(255, 34, 197, 94) : Color(255, 70, 70, 80);
  SolidBrush logBrush(logBg);
  g.FillRectangle(&logBrush, toggleLog);
  g.FillEllipse(&circleBrush,
                (REAL)(tempLogging ? toggleLog.X + 22 : toggleLog.X + 2),
                toggleLog.Y + 2, 20.0f, 20.0f);
  y += 100;

  maxScroll = (int)(y + scrollOffset - startY - contentH + 50);
  if (maxScroll < 0)
    maxScroll = 0;
}

void HandleSettingsClick(REAL mx, REAL my, RECT &rc) {
  if (!guiManager)
    return;
  REAL contentX = MARGIN_SIDEBAR + 40.0f;
  REAL sectionW = (REAL)(rc.right - rc.left) - contentX - 80.0f;
  REAL logicalY = my + scrollOffset - 100.0f;

  if (logicalY >= 40 && logicalY <= 125 && mx >= contentX + sectionW - 60) {
    SetAutoStart(!IsAutoStartEnabled());
  } else if (logicalY >= 140 && logicalY <= 225 &&
             mx >= contentX + sectionW - 60) {
    tempTray = !tempTray;
    guiManager->SetTrayIconEnabled(tempTray);
    guiManager->SaveConfig();
  } else if (logicalY >= 280 && logicalY <= 365 &&
             mx >= contentX + sectionW - 60) {
    tempAnimations = !tempAnimations;
    guiManager->SetAnimationsEnabled(tempAnimations);
    guiManager->SaveConfig();
  } else if (logicalY >= 370 && logicalY <= 455 &&
             mx >= contentX + sectionW - 60) {
    tempSounds = !tempSounds;
    guiManager->SetSoundsEnabled(tempSounds);
    guiManager->SaveConfig();
  } else if (logicalY >= 525 && logicalY <= 630) {
    if (mx >= contentX + 20 && mx <= contentX + sectionW - 20) {
      isDraggingMargin = true;
      marginSliderRect.left = (LONG)(contentX + 20);
      marginSliderRect.right = (LONG)(contentX + sectionW - 20);
      SetCapture(GetForegroundWindow());
      REAL width = (REAL)(marginSliderRect.right - marginSliderRect.left);
      tempMargin = (int)(((mx - (REAL)marginSliderRect.left) / width) * 20.0f);
      guiManager->SetMargin(tempMargin);
      guiManager->SaveConfig();
    }
  } else if (logicalY >= 640 && logicalY <= 745) {
    if (mx >= contentX + 20 && mx <= contentX + sectionW - 20) {
      isDraggingTransparency = true;
      transparencySliderRect.left = (LONG)(contentX + 20);
      transparencySliderRect.right = (LONG)(contentX + sectionW - 20);
      SetCapture(GetForegroundWindow());
      REAL width =
          (REAL)(transparencySliderRect.right - transparencySliderRect.left);
      tempTransparency =
          (int)(((mx - (REAL)transparencySliderRect.left) / width) * 255.0f);
      guiManager->SetTransparencyLevel(tempTransparency);
      guiManager->SaveConfig();
    }
  } else if (logicalY >= 770 && logicalY <= 855 &&
             mx >= contentX + sectionW - 60) {
    tempLogging = !tempLogging;
    guiManager->SetLoggingEnabled(tempLogging);
    guiManager->SaveConfig();
  }
}

// --- Window Proc ---
static LRESULT CALLBACK ConfigWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) {
  switch (uMsg) {
  case WM_CREATE: {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    if (guiManager) {
      tempSounds = guiManager->IsSoundsEnabled();
      tempAnimations = guiManager->IsAnimationsEnabled();
      tempTray = guiManager->IsTrayIconEnabled();
      tempLogging = guiManager->IsLoggingEnabled();
      tempMargin = guiManager->GetMargin();
      tempTransparency = guiManager->GetTransparencyLevel();
    }
    return 0;
  }
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBM = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    SelectObject(memDC, memBM);

    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    REAL w = (REAL)(rc.right - rc.left);
    REAL h = (REAL)(rc.bottom - rc.top);
    SolidBrush bgBrush(Color(255, 18, 18, 24));
    g.FillRectangle(&bgBrush, 0.0f, 0.0f, w, h);
    SolidBrush sideBrush(Color(255, 15, 15, 20));
    g.FillRectangle(&sideBrush, 0.0f, 0.0f, (REAL)MARGIN_SIDEBAR, h);
    Pen sideBorder(Color(40, 255, 255, 255), 1.0f);
    g.DrawLine(&sideBorder, (REAL)MARGIN_SIDEBAR, 0.0f, (REAL)MARGIN_SIDEBAR,
               h);

    FontFamily fontFamily(L"Segoe UI");
    SolidBrush whiteBrush(Color(255, 245, 245, 250));
    SolidBrush dimBrush(Color(255, 140, 140, 160));
    SolidBrush accentBrush(Color(255, 99, 102, 241));

    // Logo
    LinearGradientBrush logoBrush(PointF(25, 30), PointF(55, 60),
                                  Color(255, 99, 102, 241),
                                  Color(255, 139, 92, 246));
    g.FillEllipse(&logoBrush, 25.0f, 30.0f, 30.0f, 30.0f);
    Font fontLogo(&fontFamily, 22, FontStyleBold, UnitPixel);
    g.DrawString(L"WinVen", -1, &fontLogo, PointF(65, 35), &whiteBrush);

    // Menu
    REAL menuY = 110.0f;
    Font fontMenu(&fontFamily, 15, FontStyleRegular, UnitPixel);
    if (currentTab == 0) {
      SolidBrush activeBg(Color(40, 99, 102, 241));
      g.FillRectangle(&activeBg, 10.0f, menuY - 8, (REAL)MARGIN_SIDEBAR - 20,
                      40.0f);
      g.FillRectangle(&accentBrush, 0.0f, menuY - 8, 4.0f, 40.0f);
    }
    g.DrawString(L"  Aplicaciones", -1, &fontMenu, PointF(25, menuY),
                 currentTab == 0 ? &whiteBrush : &dimBrush);
    menuY += 50;
    if (currentTab == 1) {
      SolidBrush activeBg(Color(40, 99, 102, 241));
      g.FillRectangle(&activeBg, 10.0f, menuY - 8, (REAL)MARGIN_SIDEBAR - 20,
                      40.0f);
      g.FillRectangle(&accentBrush, 0.0f, menuY - 8, 4.0f, 40.0f);
    }
    g.DrawString(L"  Ajustes", -1, &fontMenu, PointF(25, menuY),
                 currentTab == 1 ? &whiteBrush : &dimBrush);

    REAL cx = MARGIN_SIDEBAR + 40.0f;
    REAL cy = 100.0f;
    REAL cw = w - cx - 40.0f;
    REAL ch = h - cy - 80.0f;
    Font fontTitle(&fontFamily, 28, FontStyleBold, UnitPixel);
    g.DrawString(currentTab == 0 ? L"Aplicaciones" : L"Ajustes", -1, &fontTitle,
                 PointF(cx, 35), &whiteBrush);

    if (currentTab == 0)
      PaintAppsTab(g, &fontFamily, cx, cy, cw, ch);
    else
      PaintSettingsTab(g, &fontFamily, cx, cy, cw, ch);

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
    DeleteObject(memBM);
    DeleteDC(memDC);
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_MOUSEMOVE: {
    REAL mx = (REAL)LOWORD(lParam);
    REAL my = (REAL)HIWORD(lParam);
    if (isDraggingMargin || isDraggingTransparency) {
      // Drag logic
      if (isDraggingMargin) {
        REAL width = (REAL)(marginSliderRect.right - marginSliderRect.left);
        tempMargin =
            (int)(((mx - (REAL)marginSliderRect.left) / width) * 20.0f);
        if (tempMargin < 0)
          tempMargin = 0;
        if (tempMargin > 20)
          tempMargin = 20;
        guiManager->SetMargin(tempMargin);
      }
      if (isDraggingTransparency) {
        REAL width =
            (REAL)(transparencySliderRect.right - transparencySliderRect.left);
        tempTransparency =
            (int)(((mx - (REAL)transparencySliderRect.left) / width) * 255.0f);
        if (tempTransparency < 0)
          tempTransparency = 0;
        if (tempTransparency > 255)
          tempTransparency = 255;
        guiManager->SetTransparencyLevel(tempTransparency);

        // Aplicar cambio en tiempo real a la ventana activa si ya es
        // transparente
        HWND hFg = GetForegroundWindow();
        if (hFg && (GetWindowLong(hFg, GWL_EXSTYLE) & WS_EX_LAYERED)) {
          SetLayeredWindowAttributes(hFg, 0, (BYTE)tempTransparency, LWA_ALPHA);
        }
      }
      guiManager->SaveConfig();
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }

    if (currentTab == 0) {
      int newHover = -1;
      for (const auto &card : cards) {
        if (mx >= card.rect.X && mx <= card.rect.X + card.rect.Width &&
            my >= card.rect.Y && my <= card.rect.Y + card.rect.Height) {
          newHover = card.index;
          break;
        }
      }
      if (newHover != hoveredCardIndex) {
        hoveredCardIndex = newHover;
        InvalidateRect(hwnd, NULL, FALSE);
      }
    }
    return 0;
  }
  case WM_LBUTTONUP:
    if (isDraggingMargin || isDraggingTransparency) {
      isDraggingMargin = false;
      isDraggingTransparency = false;
      ReleaseCapture();
    }
    return 0;
  case WM_LBUTTONDOWN: {
    REAL mx = (REAL)LOWORD(lParam);
    REAL my = (REAL)HIWORD(lParam);
    if (mx < MARGIN_SIDEBAR) {
      if (my >= 102 && my <= 142) {
        currentTab = 0;
        scrollOffset = 0;
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (my >= 152 && my <= 192) {
        currentTab = 1;
        scrollOffset = 0;
        InvalidateRect(hwnd, NULL, TRUE);
      }
      return 0;
    }
    if (currentTab == 1) {
      RECT rc;
      GetClientRect(hwnd, &rc);
      HandleSettingsClick(mx, my, rc);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    // App tab clicks
    for (const auto &card : cards) {
      if (mx >= card.rect.X && mx <= card.rect.X + card.rect.Width &&
          my >= card.rect.Y && my <= card.rect.Y + card.rect.Height) {
        if (card.index == -1) {
          // Open select
          selectedDiscoveryIdx = -1;
          HWND hSelect = CreateWindowExW(
              0, L"AppSelectorWin", L"Seleccionar Aplicacion",
              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT,
              CW_USEDEFAULT, 420, 530, hwnd, NULL, guiInstance, NULL);
          ShowWindow(hSelect, SW_SHOW);
          EnableWindow(hwnd, FALSE);
          MSG msg;
          while (GetMessageW(&msg, NULL, 0, 0)) {
            if (msg.hwnd == hSelect || IsChild(hSelect, msg.hwnd)) {
              TranslateMessage(&msg);
              DispatchMessageW(&msg);
            }
            if (!IsWindow(hSelect))
              break;
          }
          EnableWindow(hwnd, TRUE);
          SetForegroundWindow(hwnd);
          if (selectedDiscoveryIdx != -1) {
            capturedKey = 0;
            HWND hPrompt = CreateWindowExW(0, L"KeyPromptWin", L"Asignar Tecla",
                                           WS_OVERLAPPED | WS_CAPTION,
                                           CW_USEDEFAULT, CW_USEDEFAULT, 380,
                                           130, hwnd, NULL, guiInstance, NULL);
            ShowWindow(hPrompt, SW_SHOW);
            EnableWindow(hwnd, FALSE);
            while (GetMessageW(&msg, NULL, 0, 0)) {
              TranslateMessage(&msg);
              DispatchMessageW(&msg);
              if (!IsWindow(hPrompt))
                break;
            }
            EnableWindow(hwnd, TRUE);
            SetForegroundWindow(hwnd);
            if (capturedKey != 0) {
              guiManager->AddAppShortcut(
                  AppShortcut(discoveredApps[selectedDiscoveryIdx].name,
                              discoveredApps[selectedDiscoveryIdx].path,
                              (int)capturedKey, (MOD_CONTROL | MOD_ALT)));
              guiManager->SaveConfig();
              // Notificar al hilo principal para recargar hotkeys
              if (mainThreadId != 0) {
                PostThreadMessage(mainThreadId, WM_USER + 101, 0, 0);
              }
              InvalidateRect(hwnd, NULL, TRUE);
            }
          }
        } else {
          if (MessageBoxW(hwnd, L"Eliminar este atajo?", L"Confirmar",
                          MB_YESNO | MB_ICONQUESTION) == IDYES) {
            guiManager->RemoveAppShortcut(card.index);
            guiManager->SaveConfig();
            // Notificar al hilo principal
            if (mainThreadId != 0) {
              PostThreadMessage(mainThreadId, WM_USER + 101, 0, 0);
            }
            InvalidateRect(hwnd, NULL, TRUE);
          }
        }
        break;
      }
    }
    return 0;
  }
  case WM_MOUSEWHEEL:
    scrollOffset -= GET_WHEEL_DELTA_WPARAM(wParam) / 2;
    if (scrollOffset < 0)
      scrollOffset = 0;
    if (scrollOffset > maxScroll)
      scrollOffset = maxScroll;
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- Thread ---
DWORD WINAPI ConfigThread(LPVOID lpParam) {
  // Register Classes
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.lpfnWndProc = ConfigWindowProc;
  wc.hInstance = guiInstance;
  wc.lpszClassName = L"WinVenConfigV3";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  RegisterClassExW(&wc);

  WNDCLASSEXW wcs = {0};
  wcs.cbSize = sizeof(WNDCLASSEXW);
  wcs.lpfnWndProc = AppSelectProc;
  wcs.hInstance = guiInstance;
  wcs.lpszClassName = L"AppSelectorWin";
  wcs.hCursor = LoadCursor(NULL, IDC_ARROW);
  wcs.hbrBackground = CreateSolidBrush(RGB(25, 25, 35));
  RegisterClassExW(&wcs);

  WNDCLASSEXW wck = {0};
  wck.cbSize = sizeof(WNDCLASSEXW);
  wck.lpfnWndProc = KeyPromptProc;
  wck.hInstance = guiInstance;
  wck.lpszClassName = L"KeyPromptWin";
  wck.hCursor = LoadCursor(NULL, IDC_ARROW);
  wck.hbrBackground = CreateSolidBrush(RGB(25, 25, 35));
  RegisterClassExW(&wck);

  HWND hwnd = CreateWindowExW(
      0, L"WinVenConfigV3", L"WinVen - Configuracion",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT,
      CW_USEDEFAULT, 900, 680, NULL, NULL, guiInstance, NULL);
  ShowWindow(hwnd, SW_SHOW);

  MSG msg = {0};
  while (GetMessageW(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  // Cleanup
  UnregisterClassW(L"WinVenConfigV3", guiInstance);
  UnregisterClassW(L"AppSelectorWin", guiInstance);
  UnregisterClassW(L"KeyPromptWin", guiInstance);
  return 0;
}

void OpenConfigWindow(WindowManager *mgr, HINSTANCE hInst,
                      DWORD dwMainThreadId) {
  if (FindWindowW(L"WinVenConfigV3", NULL)) {
    // Already open, activate it
    HWND h = FindWindowW(L"WinVenConfigV3", NULL);
    if (IsIconic(h))
      ShowWindow(h, SW_RESTORE);
    SetForegroundWindow(h);
    return;
  }
  guiManager = mgr;
  guiInstance = hInst;
  mainThreadId = dwMainThreadId;
  CreateThread(NULL, 0, ConfigThread, NULL, 0, NULL);
}
