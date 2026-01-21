#include "WindowManager.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <objbase.h>
#include <set>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <sstream>
#include <tlhelp32.h>

#include <ctime>
#include <fstream>

WindowManager::WindowManager(const std::string &configPath)
    : configFile(configPath) {
  InitializePositions25();
  LoadConfig();
  if (layouts.empty()) {
    CreateDefaultLayouts();
    CreateDefaultAppShortcuts();
    SaveConfig();
  }
}

bool WindowManager::RegisterAllHotkeys(HWND messageWindow) {
  bool success = true;
  for (size_t i = 0; i < layouts.size(); ++i) {
    if (layouts[i].hotkey != 0) {
      if (!RegisterHotKey(messageWindow, 200 + i, MOD_CONTROL | MOD_ALT,
                          layouts[i].hotkey)) {
        success = false;
      }
    }
  }
  for (size_t i = 0; i < appShortcuts.size(); ++i) {
    if (appShortcuts[i].hotkey != 0) {
      if (!RegisterHotKey(messageWindow, 300 + i, appShortcuts[i].modifier,
                          appShortcuts[i].hotkey)) {
        success = false;
      } else {
      }
    }
  }
  return success;
}

WindowManager::~WindowManager() { UnregisterAllHotkeys(); }

MONITORINFO WindowManager::GetMonInfo(HWND hwnd) {
  HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  ::GetMonitorInfo(monitor, &mi);
  return mi;
}

void WindowManager::AddLayout(const WindowLayout &layout) {
  layouts.push_back(layout);
  if (layout.hotkey != 0) {
    hotkeyToLayoutIndex[layout.hotkey] = layouts.size() - 1;
  }
}

void WindowManager::AddAppShortcut(const AppShortcut &app) {
  appShortcuts.push_back(app);
  if (app.hotkey != 0) {
    hotkeyToAppIndex[app.hotkey] = appShortcuts.size() - 1;
  }
  SaveConfig();
}

void WindowManager::ExecuteAppShortcut(int hotkey) {
  auto it = hotkeyToAppIndex.find(hotkey);
  if (it != hotkeyToAppIndex.end()) {
    ExecuteAppShortcutByIndex(it->second);
  }
}

void WindowManager::ExecuteAppShortcutByIndex(int index) {
  if (index >= 0 && index < (int)appShortcuts.size()) {
    const AppShortcut &app = appShortcuts[index];
    ShellExecuteA(NULL, "open", app.path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    PlaySoundEffect(600, 100);
    std::cout << "[INFO] Ejecutando app: " << app.name << " (" << app.path
              << ")" << std::endl;
  }
}

void WindowManager::RemoveAppShortcut(int index) {
  if (index >= 0 && index < (int)appShortcuts.size()) {
    int hotkey = appShortcuts[index].hotkey;
    if (hotkey != 0) {
      hotkeyToAppIndex.erase(hotkey);
    }
    appShortcuts.erase(appShortcuts.begin() + index);
  }
  SaveConfig();
}

std::string ResolveShortcut(const std::string &shortcutPath) {
  HRESULT hres;
  IShellLinkA *psl;
  char szPath[MAX_PATH];
  szPath[0] = '\0';

  hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          IID_IShellLinkA, (LPVOID *)&psl);
  if (SUCCEEDED(hres)) {
    IPersistFile *ppf;
    hres = psl->QueryInterface(IID_IPersistFile, (LPVOID *)&ppf);
    if (SUCCEEDED(hres)) {
      WCHAR wsz[MAX_PATH];
      MultiByteToWideChar(CP_ACP, 0, shortcutPath.c_str(), -1, wsz, MAX_PATH);
      hres = ppf->Load(wsz, STGM_READ);
      if (SUCCEEDED(hres)) {
        psl->GetPath(szPath, MAX_PATH, NULL, SLGP_RAWPATH);
      }
      ppf->Release();
    }
    psl->Release();
  }
  return std::string(szPath);
}

void ScanLnkFiles(const std::string &directory,
                  std::vector<DiscoveryApp> &apps) {
  std::string searchPath = directory + "\\*";
  WIN32_FIND_DATAA fd;
  HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);

  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
          ScanLnkFiles(directory + "\\" + fd.cFileName, apps);
        }
      } else {
        std::string filename = fd.cFileName;
        if (filename.length() > 4 &&
            filename.substr(filename.length() - 4) == ".lnk") {
          std::string fullPath = directory + "\\" + filename;
          std::string target = ResolveShortcut(fullPath);
          if (!target.empty() && target.substr(target.length() - 4) == ".exe") {
            DiscoveryApp app;
            app.name = filename.substr(0, filename.length() - 4);
            app.path = target;
            apps.push_back(app);
          }
        }
      }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
  }
}

std::vector<DiscoveryApp> WindowManager::DiscoverSystemApps() {
  std::vector<DiscoveryApp> apps;
  char path[MAX_PATH];

  // Menu de inicio (Usuario)
  if (SHGetSpecialFolderPathA(NULL, path, CSIDL_PROGRAMS, FALSE)) {
    ScanLnkFiles(path, apps);
  }

  // Menu de inicio (Global)
  if (SHGetSpecialFolderPathA(NULL, path, CSIDL_COMMON_PROGRAMS, FALSE)) {
    ScanLnkFiles(path, apps);
  }

  // Desktop (Global)
  if (SHGetSpecialFolderPathA(NULL, path, CSIDL_COMMON_DESKTOPDIRECTORY,
                              FALSE)) {
    ScanLnkFiles(path, apps);
  }

  return apps;
}

void WindowManager::RemoveLayout(int index) {
  if (index >= 0 && index < (int)layouts.size()) {
    int hotkey = layouts[index].hotkey;
    if (hotkey != 0) {
      hotkeyToLayoutIndex.erase(hotkey);
    }
    layouts.erase(layouts.begin() + index);
  }
}

WindowLayout &WindowManager::GetLayout(int index) { return layouts[index]; }

void WindowManager::ApplyLayout(HWND hwnd, int layoutIndex) {
  // ✅ Validación de HWND para prevenir crashes
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }

  if (layoutIndex < 0 || layoutIndex >= (int)layouts.size()) {
    return;
  }

  WindowLayout &layout = layouts[layoutIndex];
  MONITORINFO mi = GetMonInfo(hwnd);
  RECT workArea = mi.rcWork;

  int screenW = workArea.right - workArea.left;
  int screenH = workArea.bottom - workArea.top;

  int baseX = workArea.left + (int)(screenW * layout.x);
  int baseY = workArea.top + (int)(screenH * layout.y);
  int baseW = (int)(screenW * layout.width);
  int baseH = (int)(screenH * layout.height);

  int posX = baseX + margin;
  int posY = baseY + margin;
  int width = baseW - (margin * 2);
  int height = baseH - (margin * 2);

  if (width < 100)
    width = 100;
  if (height < 100)
    height = 100;

  ShowWindow(hwnd, SW_RESTORE);
  SmoothMoveWindow(hwnd, posX, posY, width, height);
}

void WindowManager::CyclePosition25(HWND hwnd) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd) || positions25.empty())
    return;

  SaveCurrentState(hwnd);

  if (windowCycleIndex.find(hwnd) == windowCycleIndex.end()) {
    windowCycleIndex[hwnd] = 0;
  }

  int &cycleIdx = windowCycleIndex[hwnd];
  WindowLayout &layout = positions25[cycleIdx];

  MONITORINFO mi = GetMonInfo(hwnd);
  RECT workArea = mi.rcWork;
  int screenW = workArea.right - workArea.left;
  int screenH = workArea.bottom - workArea.top;

  int baseX = workArea.left + (int)(screenW * layout.x);
  int baseY = workArea.top + (int)(screenH * layout.y);
  int baseW = (int)(screenW * layout.width);
  int baseH = (int)(screenH * layout.height);

  int posX = baseX + margin;
  int posY = baseY + margin;
  int width = baseW - (margin * 2);
  int height = baseH - (margin * 2);

  if (width < 100)
    width = 100;
  if (height < 100)
    height = 100;

  ShowWindow(hwnd, SW_RESTORE);
  SmoothMoveWindow(hwnd, posX, posY, width, height);

  PlaySoundEffect(400 + (cycleIdx * 30), 30);

  cycleIdx = (cycleIdx + 1) % positions25.size();
}

void WindowManager::RestorePreviousPosition(HWND hwnd) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd) || positions25.empty())
    return;

  if (windowCycleIndex.find(hwnd) == windowCycleIndex.end()) {
    windowCycleIndex[hwnd] = 0;
  }

  int &cycleIdx = windowCycleIndex[hwnd];
  cycleIdx = (cycleIdx - 2 + (int)positions25.size()) % positions25.size();

  WindowLayout &layout = positions25[cycleIdx];

  MONITORINFO mi = GetMonInfo(hwnd);
  RECT workArea = mi.rcWork;
  int screenW = workArea.right - workArea.left;
  int screenH = workArea.bottom - workArea.top;

  int baseX = workArea.left + (int)(screenW * layout.x);
  int baseY = workArea.top + (int)(screenH * layout.y);
  int baseW = (int)(screenW * layout.width);
  int baseH = (int)(screenH * layout.height);

  int posX = baseX + margin;
  int posY = baseY + margin;
  int width = baseW - (margin * 2);
  int height = baseH - (margin * 2);

  if (width < 100)
    width = 100;
  if (height < 100)
    height = 100;

  ShowWindow(hwnd, SW_RESTORE);
  SmoothMoveWindow(hwnd, posX, posY, width, height);

  PlaySoundEffect(300, 100);

  cycleIdx = (cycleIdx + 1) % positions25.size();
}

void WindowManager::CycleLayout(HWND hwnd, bool forward) {
  if (layouts.empty())
    return;
  if (forward)
    currentCycleIndex = (currentCycleIndex + 1) % layouts.size();
  else
    currentCycleIndex =
        (currentCycleIndex - 1 + (int)layouts.size()) % layouts.size();
  ApplyLayout(hwnd, currentCycleIndex);
}

void WindowManager::TileAllWindows() {
  std::vector<HWND> windows = GetAllWindows();
  if (windows.empty())
    return;
  int count = (int)windows.size();
  int cols = (int)ceil(sqrt(count));
  int rows = (int)ceil((double)count / cols);
  HWND active = GetForegroundWindow();
  MONITORINFO mi = GetMonInfo(active ? active : GetDesktopWindow());
  RECT workArea = mi.rcWork;
  int screenW = workArea.right - workArea.left;
  int screenH = workArea.bottom - workArea.top;
  int cellW = screenW / cols;
  int cellH = screenH / rows;
  for (int i = 0; i < count; ++i) {
    int r = i / cols;
    int c = i % cols;
    int x = workArea.left + (c * cellW) + margin;
    int y = workArea.top + (r * cellH) + margin;
    int w = cellW - (margin * 2);
    int h = cellH - (margin * 2);
    ShowWindow(windows[i], SW_RESTORE);
    SmoothMoveWindow(windows[i], x, y, w, h);
  }
}

void WindowManager::ArrangeAllWindowsNoOverlap() {
  std::vector<HWND> windows = GetAllWindows();
  if (windows.empty())
    return;

  for (HWND hwnd : windows) {
    ShowWindow(hwnd, SW_RESTORE);
  }

  int count = (int)windows.size();
  if (count == 0)
    return;

  POINT pt;
  GetCursorPos(&pt);
  HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  ::GetMonitorInfo(hMon, &mi);
  RECT workArea = mi.rcWork;

  int areaW = workArea.right - workArea.left;
  int areaH = workArea.bottom - workArea.top;

  int cols = (int)ceil(sqrt(count));
  int rows = (int)ceil((double)count / cols);

  if (areaW > areaH * 2) {
    cols = count;
    rows = 1;
  }

  int cellW = areaW / cols;
  int cellH = areaH / rows;

  for (int i = 0; i < count; ++i) {
    int r = i / cols;
    int c = i % cols;

    int x = workArea.left + (c * cellW) + margin;
    int y = workArea.top + (r * cellH) + margin;
    int w = cellW - (margin * 2);
    int h = cellH - (margin * 2);

    SmoothMoveWindow(windows[i], x, y, w, h);

    FLASHWINFO fi;
    fi.cbSize = sizeof(FLASHWINFO);
    fi.hwnd = windows[i];
    fi.dwFlags = FLASHW_CAPTION;
    fi.uCount = 1;
    fi.dwTimeout = 0;
    FlashWindowEx(&fi);
  }

  PlaySoundEffect(700, 100);
}

void WindowManager::SafeCloseWindow(HWND hwnd) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd))
    return;
  PostMessage(hwnd, WM_CLOSE, 0, 0);
  PlaySoundEffect(500, 50);
}

void WindowManager::ApplyLayoutByHotkey(HWND hwnd, int hotkey) {
  auto it = hotkeyToLayoutIndex.find(hotkey);
  if (it != hotkeyToLayoutIndex.end())
    ApplyLayout(hwnd, it->second);
}

void WindowManager::PlaySoundEffect(int frequency, int duration) {
  // Sound disabled by user request
}

bool WindowManager::SaveConfig() {
  std::ofstream file(configFile);
  if (!file.is_open())
    return false;

  file << "S|" << (soundsEnabled ? "1" : "0") << "|"
       << (animationsEnabled ? "1" : "0") << "|"
       << (trayIconEnabled ? "1" : "0") << "|" << margin << "|"
       << transparencyLevel << "|" << (loggingEnabled ? "1" : "0") << "|"
       << (autoStartEnabled ? "1" : "0") << "\n";

  for (const auto &layout : layouts) {
    file << "L|" << layout.name << "|" << layout.x << "|" << layout.y << "|"
         << layout.width << "|" << layout.height << "|" << layout.hotkey
         << "\n";
  }
  for (const auto &app : appShortcuts) {
    file << "A|" << app.name << "|" << app.path << "|" << app.modifier << "|"
         << app.hotkey << "\n";
  }
  for (const auto &ex : excludedApps) {
    file << "E|" << ex << "\n";
  }
  file.close();
  return true;
}

bool WindowManager::LoadConfig() {
  std::ifstream file(configFile);
  if (!file.is_open())
    return false;
  layouts.clear();
  appShortcuts.clear();
  excludedApps.clear();
  hotkeyToLayoutIndex.clear();
  hotkeyToAppIndex.clear();
  std::string line;
  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    std::stringstream ss(line);
    std::string type;
    std::getline(ss, type, '|');

    if (type == "S") {
      std::string token;
      if (std::getline(ss, token, '|'))
        soundsEnabled = (token == "1");
      if (std::getline(ss, token, '|'))
        animationsEnabled = (token == "1");
      if (std::getline(ss, token, '|'))
        trayIconEnabled = (token == "1");
      if (std::getline(ss, token, '|'))
        margin = std::stoi(token);
      if (std::getline(ss, token, '|'))
        transparencyLevel = std::stoi(token);
      if (std::getline(ss, token, '|'))
        SetLoggingEnabled(token == "1");
      if (std::getline(ss, token, '|'))
        SetAutoStartEnabled(token == "1");
    } else if (type == "L") {
      std::string name, token;
      float x, y, width, height;
      int hotkey;
      std::getline(ss, name, '|');
      std::getline(ss, token, '|');
      x = std::stof(token);
      std::getline(ss, token, '|');
      y = std::stof(token);
      std::getline(ss, token, '|');
      width = std::stof(token);
      std::getline(ss, token, '|');
      height = std::stof(token);
      std::getline(ss, token, '|');
      hotkey = std::stoi(token);
      layouts.push_back(WindowLayout(name, x, y, width, height, hotkey));
      if (hotkey != 0)
        hotkeyToLayoutIndex[hotkey] = (int)layouts.size() - 1;
    } else if (type == "A") {
      std::string name, path, token;
      int hotkey, modifier;
      std::getline(ss, name, '|');
      std::getline(ss, path, '|');
      std::getline(ss, token, '|');
      modifier = std::stoi(token);
      if (std::getline(ss, token, '|'))
        hotkey = std::stoi(token);
      else {
        hotkey = modifier;
        modifier = MOD_CONTROL | MOD_ALT;
      }
      appShortcuts.push_back(AppShortcut(name, path, hotkey, modifier));
      if (hotkey != 0)
        hotkeyToAppIndex[hotkey] = (int)appShortcuts.size() - 1;
    } else if (type == "E") {
      std::string name;
      std::getline(ss, name);
      excludedApps.push_back(name);
    }
  }
  file.close();
  return true;
}

void WindowManager::UnregisterAllHotkeys(HWND messageWindow) {
  for (size_t i = 0; i < layouts.size(); ++i)
    UnregisterHotKey(messageWindow, 200 + i);
  for (size_t i = 0; i < appShortcuts.size(); ++i)
    UnregisterHotKey(messageWindow, 300 + i);
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  std::vector<HWND> *windows = reinterpret_cast<std::vector<HWND> *>(lParam);
  if (IsWindowVisible(hwnd)) {
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    if (!((style & WS_CAPTION) && !(style & WS_CHILD)))
      return TRUE;
    char title[256];
    if (GetWindowTextA(hwnd, title, sizeof(title)) <= 0)
      return TRUE;
    int cloaked;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked,
                                        sizeof(cloaked))) &&
        cloaked)
      return TRUE;
    if (hwnd == GetConsoleWindow())
      return TRUE;
    windows->push_back(hwnd);
  }
  return TRUE;
}

std::vector<HWND> WindowManager::GetAllWindows() {
  std::vector<HWND> windows;
  EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));
  std::vector<HWND> filtered;
  for (HWND hwnd : windows) {
    if (!IsExcluded(hwnd))
      filtered.push_back(hwnd);
  }
  return filtered;
}

std::string WindowManager::GetWindowTitle(HWND hwnd) {
  char title[256];
  GetWindowTextA(hwnd, title, sizeof(title));
  return std::string(title);
}

void WindowManager::MoveActiveWindow(HWND hwnd, int direction) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd))
    return;
  RECT r;
  GetWindowRect(hwnd, &r);
  int x = r.left, y = r.top, w = r.right - r.left, h = r.bottom - r.top;
  const int step = 60;
  switch (direction) {
  case 1:
    y -= step;
    break;
  case 2:
    y += step;
    break;
  case 3:
    x -= step;
    break;
  case 4:
    x += step;
    break;
  }
  SetWindowPos(hwnd, NULL, x, y, w, h,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowManager::ResizeActiveWindow(HWND hwnd, int direction) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd))
    return;
  RECT r;
  GetWindowRect(hwnd, &r);
  int x = r.left, y = r.top, w = r.right - r.left, h = r.bottom - r.top;
  const int step = 60;
  switch (direction) {
  case 1:
    h -= step;
    break;
  case 2:
    h += step;
    break;
  case 3:
    w -= step;
    break;
  case 4:
    w += step;
    break;
  }
  if (w < 100)
    w = 100;
  if (h < 100)
    h = 100;
  SetWindowPos(hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowManager::SaveSession() {
  std::ofstream file("session.cfg");
  if (!file.is_open())
    return;
  std::vector<HWND> windows = GetAllWindows();
  for (HWND hwnd : windows) {
    RECT r;
    GetWindowRect(hwnd, &r);
    file << GetWindowTitle(hwnd) << "|" << r.left << "|" << r.top << "|"
         << (r.right - r.left) << "|" << (r.bottom - r.top) << "\n";
  }
  file.close();
}

void WindowManager::SaveCurrentState(HWND hwnd) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd))
    return;
  RECT r;
  if (GetWindowRect(hwnd, &r)) {
    WindowState state;
    state.startTime = GetTickCount(); // Unused but initialized
    state.rect = r;
    previousStates[hwnd] = state;
  }
}

void WindowManager::RestoreSession() {
  std::ifstream file("session.cfg");
  if (!file.is_open())
    return;
  std::string line;
  std::vector<HWND> openWindows = GetAllWindows();
  while (std::getline(file, line)) {
    if (line.empty())
      continue;
    std::stringstream ss(line);
    std::string title, token;
    int x, y, w, h;
    std::getline(ss, title, '|');
    std::getline(ss, token, '|');
    x = std::stoi(token);
    std::getline(ss, token, '|');
    y = std::stoi(token);
    std::getline(ss, token, '|');
    w = std::stoi(token);
    std::getline(ss, token, '|');
    h = std::stoi(token);
    for (HWND hwnd : openWindows) {
      if (GetWindowTitle(hwnd) == title) {
        SmoothMoveWindow(hwnd, x, y, w, h);
        break;
      }
    }
  }
}

void WindowManager::SwitchWindowFocus(bool forward) {
  std::vector<HWND> windows = GetAllWindows();
  if (windows.empty())
    return;

  if (windows.size() == 1) {
    SetForegroundWindow(windows[0]);
    return;
  }

  HWND current = GetForegroundWindow();
  int currentIndex = -1;

  for (int i = 0; i < (int)windows.size(); ++i) {
    if (windows[i] == current) {
      currentIndex = i;
      break;
    }
  }

  if (currentIndex == -1) {
    currentIndex = 0;
  }

  int nextIndex =
      forward ? (currentIndex + 1) % windows.size()
              : (currentIndex - 1 + (int)windows.size()) % windows.size();

  HWND nextWindow = windows[nextIndex];
  DWORD currentThreadId = GetCurrentThreadId();
  DWORD targetThreadId = GetWindowThreadProcessId(nextWindow, NULL);

  bool attached = false;
  if (currentThreadId != targetThreadId) {
    attached = AttachThreadInput(currentThreadId, targetThreadId, TRUE);
  }

  AllowSetForegroundWindow(ASFW_ANY);

  if (IsIconic(nextWindow)) {
    ShowWindow(nextWindow, SW_RESTORE);
  }

  SetWindowPos(nextWindow, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);

  SetForegroundWindow(nextWindow);
  SetActiveWindow(nextWindow);
  BringWindowToTop(nextWindow);
  SetFocus(nextWindow);

  if (attached) {
    AttachThreadInput(currentThreadId, targetThreadId, FALSE);
  }

  FLASHWINFO fi;
  fi.cbSize = sizeof(FLASHWINFO);
  fi.hwnd = nextWindow;
  fi.dwFlags = FLASHW_CAPTION;
  fi.uCount = 1;
  fi.dwTimeout = 100;
  FlashWindowEx(&fi);
}

void WindowManager::ShowMissionControl() {
  INPUT inputs[8] = {0};
  int n = 0;
  inputs[n].type = INPUT_KEYBOARD;
  inputs[n].ki.wVk = VK_CONTROL;
  inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
  n++;
  inputs[n].type = INPUT_KEYBOARD;
  inputs[n].ki.wVk = VK_MENU;
  inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
  n++;
  inputs[n].type = INPUT_KEYBOARD;
  inputs[n].ki.wVk = VK_LWIN;
  n++;
  inputs[n].type = INPUT_KEYBOARD;
  inputs[n].ki.wVk = VK_TAB;
  n++;
  inputs[n].type = INPUT_KEYBOARD;
  inputs[n].ki.wVk = VK_TAB;
  inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
  n++;
  inputs[n].type = INPUT_KEYBOARD;
  inputs[n].ki.wVk = VK_LWIN;
  inputs[n].ki.dwFlags = KEYEVENTF_KEYUP;
  n++;
  SendInput(n, inputs, sizeof(INPUT));
}

void WindowManager::CenterWindow(HWND hwnd) {
  MONITORINFO mi = GetMonInfo(hwnd);
  RECT wa = mi.rcWork;
  int w = (int)((wa.right - wa.left) * 0.8f),
      h = (int)((wa.bottom - wa.top) * 0.8f);
  int x = wa.left + (wa.right - wa.left - w) / 2,
      y = wa.top + (wa.bottom - wa.top - h) / 2;
  SetWindowPos(hwnd, HWND_TOP, x, y, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);
}

void WindowManager::ToggleTransparency(HWND hwnd) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd))
    return;
  LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (!(style & WS_EX_LAYERED)) {
    SetWindowLong(hwnd, GWL_EXSTYLE, style | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)transparencyLevel, LWA_ALPHA);
    PlaySoundEffect(800, 50);
  } else {
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    SetWindowLong(hwnd, GWL_EXSTYLE, style & ~WS_EX_LAYERED);
    PlaySoundEffect(600, 50);
  }
}

void WindowManager::ToggleAlwaysOnTop(HWND hwnd) {
  // ✅ Validación de HWND
  if (!hwnd || !IsWindow(hwnd))
    return;
  LONG style = GetWindowLong(hwnd, GWL_EXSTYLE);
  if (style & WS_EX_TOPMOST) {
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    PlaySoundEffect(500, 50);
  } else {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    PlaySoundEffect(900, 50);
  }
}

// ===== GAME MODE IMPLEMENTATION =====

LRESULT CALLBACK GameModeWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                 LPARAM lParam) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT r;
    GetClientRect(hwnd, &r);

    // Transparent background text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 255, 0)); // Green text for visibility

    HFONT hFont =
        CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                    DEFAULT_PITCH | FF_SWISS, "Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    TextOutA(hdc, 2, 0, "JUEGO", 5);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    EndPaint(hwnd, &ps);
    return 0;
  }
  // Handle destruction to avoid zombie window classes if needed, though we use
  // a static class
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void KillProcessByName(const char *filename) {
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE)
    return;

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);

  if (Process32First(hSnap, &pe32)) {
    do {
      if (_stricmp(pe32.szExeFile, filename) == 0) {
        HANDLE hProcess =
            OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
        if (hProcess) {
          TerminateProcess(hProcess, 0);
          CloseHandle(hProcess);
        }
      }
    } while (Process32Next(hSnap, &pe32));
  }
  CloseHandle(hSnap);
}

void WindowManager::ToggleGameMode() {
  isGameMode = !isGameMode;
  if (isGameMode) {
    ShowGameModeIndicator();
    // CloseNonEssentialApps(); // DESACTIVADO POR PETICION DEL USUARIO
    PlaySoundEffect(1000, 150); // High pitch ON
  } else {
    HideGameModeIndicator();
    PlaySoundEffect(500, 150); // Low pitch OFF
  }
}

void WindowManager::ShowGameModeIndicator() {
  if (gameModeIndicatorHwnd && IsWindow(gameModeIndicatorHwnd))
    return;

  static bool classRegistered = false;
  const char *className = "GameModeIndClass";

  if (!classRegistered) {
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = GameModeWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassExA(&wc);
    classRegistered = true;
  }

  // Create a layered window at top-left
  gameModeIndicatorHwnd = CreateWindowExA(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
      className, "GameMode", WS_POPUP | WS_VISIBLE, 5, 5, 60, 20, // x, y, w, h
      NULL, NULL, GetModuleHandle(NULL), NULL);

  // Set transparency key (though we might just want full opacity for text and 0
  // for bg) Using LWA_COLORKEY to make a specific background color transparent
  // is common, but here we used NULL_BRUSH. Let's use 255 alpha (opaque) but
  // relying on WM_PAINT for valid pixels. Actually, for simple text overlay,
  // SetLayeredWindowAttributes with LWA_COLORKEY is easiest if we paint a bg
  // color. Re-adjusting: Paint black bg, set black as colorkey.

  // Set window opacity
  SetLayeredWindowAttributes(gameModeIndicatorHwnd, 0, 255, LWA_ALPHA);

  SetWindowPos(gameModeIndicatorHwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WindowManager::HideGameModeIndicator() {
  if (gameModeIndicatorHwnd) {
    if (IsWindow(gameModeIndicatorHwnd))
      DestroyWindow(gameModeIndicatorHwnd);
    gameModeIndicatorHwnd = NULL;
  }
}

void WindowManager::CloseNonEssentialApps() {
  std::vector<HWND> windows = GetAllWindows();
  HWND foreground = GetForegroundWindow();
  HWND console = GetConsoleWindow();
  DWORD currentPid = GetCurrentProcessId();
  DWORD foregroundPid = 0;
  if (foreground) {
    GetWindowThreadProcessId(foreground, &foregroundPid);
  }

  // 1. Cerrar Ventanas Visibles (Modo Cortés)
  int closedCount = 0;
  for (HWND hwnd : windows) {
    if (hwnd == foreground)
      continue; // Don't close the active game/app
    if (hwnd == gameModeIndicatorHwnd)
      continue; // Don't close our indicator
    if (hwnd == console)
      continue; // Don't close ourself if visible

    DWORD wndPid = 0;
    GetWindowThreadProcessId(hwnd, &wndPid);
    if (wndPid == currentPid)
      continue; // Don't close windows owned by this process

    char className[256];
    if (GetClassNameA(hwnd, className, sizeof(className))) {
      // Protect Shell/System windows
      if (strcmp(className, "Shell_TrayWnd") == 0 ||
          strcmp(className, "Progman") == 0 ||
          strcmp(className, "WorkerW") == 0) {
        continue;
      }
    }

    PostMessage(hwnd, WM_CLOSE, 0, 0);
    closedCount++;
  }

  // 2. Terminar Procesos en Segundo Plano (Modo Agresivo)
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    // Whitelist de procesos del sistema y esenciales
    std::set<std::string> whitelist = {"explorer.exe",
                                       "svchost.exe",
                                       "csrss.exe",
                                       "wininit.exe",
                                       "services.exe",
                                       "lsass.exe",
                                       "winlogon.exe",
                                       "dwm.exe",
                                       "smss.exe",
                                       "taskhostw.exe",
                                       "RuntimeBroker.exe",
                                       "sihost.exe",
                                       "ctfmon.exe",
                                       "smartscreen.exe",
                                       "conhost.exe",
                                       "System",
                                       "registry",
                                       "audiodg.exe",
                                       "spoolsv.exe",
                                       "dasHost.exe",
                                       "SearchUI.exe",
                                       "ShellExperienceHost.exe",
                                       "StandardCollector.Service.exe",
                                       "WmiPrvSE.exe",
                                       "Memory Compression",
                                       "ntoskrnl.exe",
                                       "gestor_ven.exe",
                                       "ConfiguracionWinVen.exe",
                                       "cmd.exe"};

    if (Process32First(hSnap, &pe32)) {
      do {
        // Ignorar el proceso actual y el juego activo
        if (pe32.th32ProcessID == currentPid ||
            pe32.th32ProcessID == foregroundPid || pe32.th32ProcessID == 0) {
          continue;
        }

        std::string exeName = pe32.szExeFile;
        // Case-insensitive check (simple tolower)
        std::string exeNameLower = exeName;
        std::transform(exeNameLower.begin(), exeNameLower.end(),
                       exeNameLower.begin(), ::tolower);

        bool isSafe = false;
        for (const auto &safe : whitelist) {
          std::string safeLower = safe;
          std::transform(safeLower.begin(), safeLower.end(), safeLower.begin(),
                         ::tolower);
          if (exeNameLower == safeLower) {
            isSafe = true;
            break;
          }
        }

        if (isSafe)
          continue;

        // Intentar terminar el proceso
        HANDLE hProcess =
            OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
        if (hProcess) {
          TerminateProcess(hProcess, 0);
          CloseHandle(hProcess);
          std::cout << "[GAME MODE] Proceso terminado: " << exeName
                    << std::endl;
        }

      } while (Process32Next(hSnap, &pe32));
    }
    CloseHandle(hSnap);
  }

  std::cout << "[GAME MODE] Cerradas " << closedCount
            << " ventanas no esenciales y procesos de fondo." << std::endl;
}

void WindowManager::SmoothMoveWindow(HWND hwnd, int tx, int ty, int tw,
                                     int th) {
  if (!hwnd)
    return;

  if (!animationsEnabled) {
    SetWindowPos(hwnd, NULL, tx, ty, tw, th, SWP_NOZORDER | SWP_NOACTIVATE);
    return;
  }

  RECT sr;
  GetWindowRect(hwnd, &sr);
  int sx = sr.left, sy = sr.top, sw = sr.right - sr.left,
      sh = sr.bottom - sr.top;
  if (sx == tx && sy == ty && sw == tw && sh == th)
    return;
  for (int i = 1; i <= 12; ++i) {
    float f = sin(((float)i / 12) * (3.14159f / 2.0f));
    SetWindowPos(hwnd, NULL, sx + (int)((tx - sx) * f),
                 sy + (int)((ty - sy) * f), sw + (int)((tw - sw) * f),
                 sh + (int)((th - sh) * f),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
    Sleep(10);
  }
  SetWindowPos(hwnd, NULL, tx, ty, tw, th, SWP_NOZORDER | SWP_NOACTIVATE);
}

void WindowManager::TileMasterStack() {
  std::vector<HWND> windows = GetAllWindows();
  if (windows.empty())
    return;
  HWND active = GetForegroundWindow();
  MONITORINFO mi = GetMonInfo(active ? active : GetDesktopWindow());
  RECT wa = mi.rcWork;
  int sw = wa.right - wa.left, sh = wa.bottom - wa.top;
  if (windows.size() == 1) {
    ApplyLayout(windows[0], 21);
    return;
  }
  int mw = (int)(sw * 0.6), stw = sw - mw;
  ShowWindow(windows[0], SW_RESTORE);
  SmoothMoveWindow(windows[0], wa.left + margin, wa.top + margin,
                   mw - (margin * 2), sh - (margin * 2));
  int sth = sh / (windows.size() - 1);
  for (int i = 1; i < (int)windows.size(); ++i) {
    ShowWindow(windows[i], SW_RESTORE);
    SmoothMoveWindow(windows[i], wa.left + mw + margin,
                     wa.top + ((i - 1) * sth) + margin, stw - (margin * 2),
                     sth - (margin * 2));
  }
}

struct MData {
  std::vector<RECT> m;
};
BOOL CALLBACK EMonProc(HMONITOR hm, HDC hdc, LPRECT lr, LPARAM d) {
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfo(hm, &mi); // API Windows
  reinterpret_cast<MData *>(d)->m.push_back(mi.rcWork);
  return TRUE;
}

void WindowManager::MoveWindowToMonitor(HWND hwnd, bool next) {
  if (!hwnd)
    return;
  MData d;
  EnumDisplayMonitors(NULL, NULL, EMonProc, (LPARAM)&d);
  if (d.m.size() < 2)
    return;
  HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfo(hm, &mi); // API Windows
  int ci = -1;
  for (int i = 0; i < (int)d.m.size(); ++i) {
    if (d.m[i].left == mi.rcWork.left && d.m[i].top == mi.rcWork.top) {
      ci = i;
      break;
    }
  }
  if (ci == -1)
    return;
  int ni = next ? (ci + 1) % d.m.size() : (ci - 1 + d.m.size()) % d.m.size();
  RECT nw = d.m[ni], cw = mi.rcWork, wr;
  GetWindowRect(hwnd, &wr);
  float rx = (float)(wr.left - cw.left) / (cw.right - cw.left),
        ry = (float)(wr.top - cw.top) / (cw.bottom - cw.top);
  float rw = (float)(wr.right - wr.left) / (cw.right - cw.left),
        rh = (float)(wr.bottom - wr.top) / (cw.bottom - cw.top);
  ShowWindow(hwnd, SW_RESTORE);
  SmoothMoveWindow(hwnd, nw.left + (int)(rx * (nw.right - nw.left)),
                   nw.top + (int)(ry * (nw.bottom - nw.top)),
                   (int)(rw * (nw.right - nw.left)),
                   (int)(rh * (nw.bottom - nw.top)));
}

void WindowManager::BringToFront(HWND hwnd) {
  if (hwnd) {
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(hwnd);
  }
}

void WindowManager::SendToBack(HWND hwnd) {
  if (hwnd) {
    SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
}

void WindowManager::InitializePositions25() {
  positions25.clear();
  // FILA 1
  positions25.push_back(WindowLayout("1.1", 0.0f, 0.0f, 0.5f, 1.0f));
  positions25.push_back(WindowLayout("1.2", 0.0f, 0.0f, 0.33f, 1.0f));
  positions25.push_back(WindowLayout("1.3", 0.0f, 0.0f, 0.25f, 1.0f));
  positions25.push_back(WindowLayout("1.4", 0.0f, 0.2f, 0.5f, 0.6f));
  positions25.push_back(WindowLayout("1.5", 0.0f, 0.1f, 0.4f, 0.8f));
  // FILA 2
  positions25.push_back(WindowLayout("2.1", 0.5f, 0.0f, 0.5f, 1.0f));
  positions25.push_back(WindowLayout("2.2", 0.67f, 0.0f, 0.33f, 1.0f));
  positions25.push_back(WindowLayout("2.3", 0.75f, 0.0f, 0.25f, 1.0f));
  positions25.push_back(WindowLayout("2.4", 0.5f, 0.2f, 0.5f, 0.6f));
  positions25.push_back(WindowLayout("2.5", 0.6f, 0.1f, 0.4f, 0.8f));
  // FILA 3
  positions25.push_back(WindowLayout("3.1", 0.15f, 0.1f, 0.7f, 0.8f));
  positions25.push_back(WindowLayout("3.2", 0.25f, 0.25f, 0.5f, 0.5f));
  positions25.push_back(WindowLayout("3.3", 0.1f, 0.1f, 0.8f, 0.8f));
  positions25.push_back(WindowLayout("3.4", 0.0f, 0.0f, 1.0f, 1.0f));
  positions25.push_back(
      WindowLayout("3.5", 0.1f, 0.1f, 0.5f, 0.5f)); // Flotante
  // FILA 4
  positions25.push_back(WindowLayout("4.1", 0.0f, 0.0f, 1.0f, 0.5f));
  positions25.push_back(WindowLayout("4.2", 0.0f, 0.5f, 1.0f, 0.5f));
  positions25.push_back(WindowLayout("4.3", 0.0f, 0.0f, 1.0f, 0.33f));
  positions25.push_back(WindowLayout("4.4", 0.0f, 0.33f, 1.0f, 0.34f));
  positions25.push_back(WindowLayout("4.5", 0.0f, 0.67f, 1.0f, 0.33f));
  // FILA 5
  positions25.push_back(WindowLayout("5.1", 0.0f, 0.0f, 0.5f, 0.5f));
  positions25.push_back(WindowLayout("5.2", 0.5f, 0.0f, 0.5f, 0.5f));
  positions25.push_back(WindowLayout("5.3", 0.0f, 0.5f, 0.5f, 0.5f));
  positions25.push_back(WindowLayout("5.4", 0.5f, 0.5f, 0.5f, 0.5f));
  positions25.push_back(WindowLayout("5.5", 0.33f, 0.33f, 0.33f, 0.33f));
}

void WindowManager::AddToExclusionList(const std::string &processName) {
  excludedApps.push_back(processName);
  SaveConfig();
}

bool WindowManager::IsExcluded(HWND hwnd) {
  if (!hwnd)
    return false;
  DWORD pid;
  GetWindowThreadProcessId(hwnd, &pid);
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnapshot == INVALID_HANDLE_VALUE)
    return false;

  PROCESSENTRY32 pe32;
  pe32.dwSize = sizeof(PROCESSENTRY32);
  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (pe32.th32ProcessID == pid) {
        std::string exeName = pe32.szExeFile;
        for (const auto &excluded : excludedApps) {
          if (exeName.find(excluded) != std::string::npos) {
            CloseHandle(hSnapshot);
            return true;
          }
        }
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
  return false;
}

void WindowManager::CreateDefaultLayouts() {
  layouts.clear();
  AddLayout(WindowLayout("Mitad Izquierda", 0.0f, 0.0f, 0.5f, 1.0f));
  AddLayout(WindowLayout("Mitad Derecha", 0.5f, 0.0f, 0.5f, 1.0f));
  AddLayout(WindowLayout("Mitad Superior", 0.0f, 0.0f, 1.0f, 0.5f));
  AddLayout(WindowLayout("Mitad Inferior", 0.0f, 0.5f, 1.0f, 0.5f));
  AddLayout(WindowLayout("Arriba-Izquierda", 0.0f, 0.0f, 0.5f, 0.5f));
  AddLayout(WindowLayout("Arriba-Derecha", 0.5f, 0.0f, 0.5f, 0.5f));
  AddLayout(WindowLayout("Abajo-Izquierda", 0.0f, 0.5f, 0.5f, 0.5f));
  AddLayout(WindowLayout("Abajo-Derecha", 0.5f, 0.5f, 0.5f, 0.5f));
  AddLayout(WindowLayout("Tercio Izquierdo", 0.0f, 0.0f, 0.333f, 1.0f));
  AddLayout(WindowLayout("Tercio Medio", 0.333f, 0.0f, 0.334f, 1.0f));
  AddLayout(WindowLayout("Tercio Derecho", 0.667f, 0.0f, 0.333f, 1.0f));
  AddLayout(WindowLayout("Tercio Superior", 0.0f, 0.0f, 1.0f, 0.333f));
  AddLayout(
      WindowLayout("Tercio Horizontal Medio", 0.0f, 0.333f, 1.0f, 0.334f));
  AddLayout(WindowLayout("Tercio Inferior", 0.0f, 0.667f, 1.0f, 0.333f));
  AddLayout(WindowLayout("2 tercios Izquierda", 0.0f, 0.0f, 0.667f, 1.0f));
  AddLayout(WindowLayout("2 tercios Derecha", 0.333f, 0.0f, 0.667f, 1.0f));
  AddLayout(WindowLayout("Centro Ancho (70%)", 0.15f, 0.0f, 0.7f, 1.0f));
  AddLayout(WindowLayout("Centro Alto (70%)", 0.0f, 0.15f, 1.0f, 0.7f));
  AddLayout(WindowLayout("Barra Lateral Izq", 0.0f, 0.0f, 0.25f, 1.0f));
  AddLayout(WindowLayout("Barra Lateral Der", 0.75f, 0.0f, 0.25f, 1.0f));
  AddLayout(WindowLayout("Centro Cine/Lectura", 0.15f, 0.1f, 0.7f, 0.8f));
  AddLayout(WindowLayout("Maximizado Pro", 0.0f, 0.0f, 1.0f, 1.0f));
}

void WindowManager::CreateDefaultAppShortcuts() { appShortcuts.clear(); }

// --- Nuevas funciones de configuración ---
#include "Logger.h"
void WindowManager::SetLoggingEnabled(bool enabled) {
  loggingEnabled = enabled;
  WinVenLogger::SetEnabled(enabled);
}

void WindowManager::SetAutoStartEnabled(bool enabled) {
  autoStartEnabled = enabled;
  HKEY hKey;
  if (RegOpenKeyExA(HKEY_CURRENT_USER,
                    "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                    KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
    if (enabled) {
      char path[MAX_PATH];
      GetModuleFileNameA(NULL, path, MAX_PATH);
      RegSetValueExA(hKey, "WinVen", 0, REG_SZ, (BYTE *)path,
                     (DWORD)(strlen(path) + 1));
    } else {
      RegDeleteValueA(hKey, "WinVen");
    }
    RegCloseKey(hKey);
  }
}
