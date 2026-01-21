#include "ConfigGUI.h"
#include "ConfigManager.h"
#include "HotkeyManager.h"
#include "Logger.h"
#include "WindowManager.h"
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <gdiplus.h>
#include <iostream>
#include <shellapi.h> // Necesario para ShellExecuteA
#include <string>
#include <vector>

// Configuración de movimiento/redimensionado
#define MOVE_STEP 15        // Pixeles por paso
#define RESIZE_STEP 15      // Pixeles por paso
#define CONTINUOUS_DELAY 16 // ~60 FPS (16ms)
#define INITIAL_DELAY 150   // Delay inicial antes de movimiento continuo
#define MIN_WINDOW_SIZE 100 // Tamano minimo de ventana

// Estado global para control continuo
bool isMoving = false;
bool isResizing = false;

// Función para mover ventana suavemente
void MoveWindowSmooth(HWND hwnd, int dx, int dy) {
  if (!hwnd)
    return;

  RECT r;
  GetWindowRect(hwnd, &r);

  int newX = r.left + dx;
  int newY = r.top + dy;
  int width = r.right - r.left;
  int height = r.bottom - r.top;

  // Obtener límites del monitor
  HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfo(hMon, &mi);
  RECT workArea = mi.rcWork;

  // Limitar a los bordes del monitor (permitir que parte de la ventana salga)
  if (newX < workArea.left - width + 50)
    newX = workArea.left - width + 50;
  if (newY < workArea.top - height + 50)
    newY = workArea.top - height + 50;
  if (newX > workArea.right - 50)
    newX = workArea.right - 50;
  if (newY > workArea.bottom - 50)
    newY = workArea.bottom - 50;

  SetWindowPos(hwnd, NULL, newX, newY, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Función para redimensionar ventana suavemente
void ResizeWindowSmooth(HWND hwnd, int dw, int dh, int anchorX, int anchorY) {
  if (!hwnd)
    return;

  RECT r;
  GetWindowRect(hwnd, &r);

  int x = r.left;
  int y = r.top;
  int width = r.right - r.left;
  int height = r.bottom - r.top;

  if (anchorX == -1) {
    x += dw;
    width -= dw;
  } else if (anchorX == 1) {
    width += dw;
  }

  if (anchorY == -1) {
    y += dh;
    height -= dh;
  } else if (anchorY == 1) {
    height += dh;
  }

  if (width < MIN_WINDOW_SIZE) {
    if (anchorX == -1)
      x -= (MIN_WINDOW_SIZE - width);
    width = MIN_WINDOW_SIZE;
  }
  if (height < MIN_WINDOW_SIZE) {
    if (anchorY == -1)
      y -= (MIN_WINDOW_SIZE - height);
    height = MIN_WINDOW_SIZE;
  }

  HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {sizeof(mi)};
  GetMonitorInfo(hMon, &mi);
  RECT workArea = mi.rcWork;
  int maxWidth = workArea.right - workArea.left;
  int maxHeight = workArea.bottom - workArea.top;

  if (width > maxWidth)
    width = maxWidth;
  if (height > maxHeight)
    height = maxHeight;

  SetWindowPos(hwnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

// Thread para manejar movimiento continuo con Ctrl + WASD
DWORD WINAPI ContinuousControlThread(LPVOID lpParam) {
  WindowManager *manager = (WindowManager *)lpParam;

  bool wasMoving = false;
  bool wasResizing = false;
  DWORD moveStartTime = 0;
  DWORD resizeStartTime = 0;

  while (true) {
    bool ctrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool altPressed = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool shiftPressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    bool keyA = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool keyW = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool keyS = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool keyD = (GetAsyncKeyState('D') & 0x8000) != 0;

    // Si el modo juego está activado, deshabilitar movimientos continuos
    if (manager->IsGameMode()) {
      isMoving = false;
      isResizing = false;
      Sleep(100);
      continue;
    }

    HWND hwnd = GetForegroundWindow();

    // ===== MODO MOVIMIENTO: Ctrl + WASD =====
    if (ctrlPressed && !altPressed) {
      bool localMove = false;
      int dx = 0, dy = 0;

      if (!shiftPressed) {
        if (keyA) {
          dx -= MOVE_STEP;
          localMove = true;
        }
        if (keyD) {
          dx += MOVE_STEP;
          localMove = true;
        }
        if (keyW) {
          dy -= MOVE_STEP;
          localMove = true;
        }
      }

      if (shiftPressed && keyS) {
        dy += MOVE_STEP;
        localMove = true;
      }

      if (localMove) {
        if (!wasMoving) {
          moveStartTime = GetTickCount();
          wasMoving = true;
        }
        DWORD elapsed = GetTickCount() - moveStartTime;
        if (elapsed > INITIAL_DELAY || elapsed == 0) {
          if (hwnd && (dx != 0 || dy != 0)) {
            MoveWindowSmooth(hwnd, dx, dy);
          }
        }
      }
      isMoving = true;
    } else {
      wasMoving = false;
      isMoving = false;
    }

    // ===== MODO REDIMENSIONADO: Alt + WASD =====
    if (altPressed && !ctrlPressed && !shiftPressed &&
        (keyA || keyW || keyS || keyD)) {
      if (!wasResizing) {
        resizeStartTime = GetTickCount();
        wasResizing = true;
      }
      DWORD elapsed = GetTickCount() - resizeStartTime;
      if (elapsed > INITIAL_DELAY || elapsed == 0) {
        if (hwnd) {
          if (keyA)
            ResizeWindowSmooth(hwnd, -RESIZE_STEP, 0, 1, 0);
          if (keyD)
            ResizeWindowSmooth(hwnd, RESIZE_STEP, 0, 1, 0);
          if (keyW)
            ResizeWindowSmooth(hwnd, 0, -RESIZE_STEP, 0, 1);
          if (keyS)
            ResizeWindowSmooth(hwnd, 0, RESIZE_STEP, 0, 1);
        }
      }
      isResizing = true;
    } else if (altPressed && shiftPressed && !ctrlPressed &&
               (keyA || keyW || keyS || keyD)) {
      // ===== MODO REDIMENSIONADO INVERSO: Alt + Shift + WASD =====
      if (!wasResizing) {
        resizeStartTime = GetTickCount();
        wasResizing = true;
      }
      DWORD elapsed = GetTickCount() - resizeStartTime;
      if (elapsed > INITIAL_DELAY || elapsed == 0) {
        if (hwnd) {
          if (keyA)
            ResizeWindowSmooth(hwnd, -RESIZE_STEP, 0, -1, 0);
          if (keyD)
            ResizeWindowSmooth(hwnd, RESIZE_STEP, 0, -1, 0);
          if (keyW)
            ResizeWindowSmooth(hwnd, 0, -RESIZE_STEP, 0, -1);
          if (keyS)
            ResizeWindowSmooth(hwnd, 0, RESIZE_STEP, 0, -1);
        }
      }
      isResizing = true;
    } else {
      wasResizing = false;
      isResizing = false;
    }

    // OPTIMIZACION: Sleep adaptativo
    bool hasActivity =
        (keyA || keyW || keyS || keyD) && (ctrlPressed || altPressed);
    Sleep(hasActivity ? CONTINUOUS_DELAY : 100);
  }
  return 0;
}

void RunWorker() {
  // 1. Obtener ruta del ejecutable
  char szExePath[MAX_PATH];
  GetModuleFileNameA(NULL, szExePath, MAX_PATH);
  std::string exeDir = std::string(szExePath).substr(
      0, std::string(szExePath).find_last_of("\\/"));

  // 2. Inicializar Logger
  WinVenLogger::SetLogFile((exeDir + "\\winven.log").c_str());
  WinVenLogger::SetMinLevel(WinVenLogger::L_INFO);
  LOG_INFO("=== Iniciando WinVen Service ===");

  // Initialize GDI+
  ULONG_PTR gdiplusToken;
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  ShowWindow(GetConsoleWindow(), SW_HIDE);

  // 3. Inicializar Configuración
  ConfigManager configMgr(exeDir + "\\config.json");
  configMgr.Load();

  if (!configMgr.Exists("hotkeys.config_panel")) {
    configMgr.SetString("hotkeys.config_panel", "Ctrl+Alt+0");
    configMgr.SetString("hotkeys.game_mode", "Ctrl+Alt+J");
    configMgr.Save();
  }

  // 4. Inicializar Gestores
  WindowManager manager(exeDir + "\\window_layouts.cfg");
  manager.LoadConfig();
  HotkeyManager hotkeyMgr;
  DWORD mainThreadId = GetCurrentThreadId();

  HANDLE hThread =
      CreateThread(NULL, 0, ContinuousControlThread, &manager, 0, NULL);
  if (hThread) {
    SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);
  }

  std::string configHk =
      configMgr.GetString("hotkeys.config_panel", "Ctrl+Alt+0");
  std::string gameModeHk =
      configMgr.GetString("hotkeys.game_mode", "Ctrl+Alt+J");

  auto checkGameMode = [&](std::function<void()> action) {
    if (!manager.IsGameMode()) {
      action();
    }
  };

  if (configHk == gameModeHk) {
    hotkeyMgr.RegisterHotkey(
        HotkeyManager::HK_GAME_MODE, configHk, [&](int id) {
          bool wasGameMode = manager.IsGameMode();
          manager.ToggleGameMode();
          if (wasGameMode && !manager.IsGameMode()) {
            OpenConfigWindow(&manager, GetModuleHandle(NULL), mainThreadId);
          }
        });
  } else {
    hotkeyMgr.RegisterHotkey(HotkeyManager::HK_OPEN_CONFIG, configHk, [&](int) {
      checkGameMode([&]() {
        manager.PlaySoundEffect(750, 100);
        OpenConfigWindow(&manager, GetModuleHandle(NULL), mainThreadId);
      });
    });
    hotkeyMgr.RegisterHotkey(HotkeyManager::HK_GAME_MODE, gameModeHk,
                             [&](int) { manager.ToggleGameMode(); });
  }

  // Navegación
  hotkeyMgr.RegisterHotkey(
      HotkeyManager::HK_NAV_LEFT, MOD_CONTROL | MOD_ALT, VK_LEFT,
      [&](int) { checkGameMode([&]() { manager.SwitchWindowFocus(false); }); });
  hotkeyMgr.RegisterHotkey(
      HotkeyManager::HK_NAV_RIGHT, MOD_CONTROL | MOD_ALT, VK_RIGHT,
      [&](int) { checkGameMode([&]() { manager.SwitchWindowFocus(true); }); });
  hotkeyMgr.RegisterHotkey(HotkeyManager::HK_NAV_UP, MOD_CONTROL | MOD_ALT,
                           VK_UP, [&](int) {
                             checkGameMode([&]() {
                               HWND h = GetForegroundWindow();
                               if (h)
                                 manager.BringToFront(h);
                             });
                           });
  hotkeyMgr.RegisterHotkey(HotkeyManager::HK_NAV_DOWN, MOD_CONTROL | MOD_ALT,
                           VK_DOWN, [&](int) {
                             checkGameMode([&]() {
                               HWND h = GetForegroundWindow();
                               if (h)
                                 manager.SendToBack(h);
                             });
                           });

  // Gestión avanzada
  hotkeyMgr.RegisterHotkey(HotkeyManager::HK_CYCLE_25, MOD_CONTROL | MOD_ALT,
                           '1', [&](int) {
                             checkGameMode([&]() {
                               HWND h = GetForegroundWindow();
                               if (h)
                                 manager.CyclePosition25(h);
                             });
                           });
  hotkeyMgr.RegisterHotkey(HotkeyManager::HK_RESTORE_POS, MOD_CONTROL | MOD_ALT,
                           '2', [&](int) {
                             checkGameMode([&]() {
                               HWND h = GetForegroundWindow();
                               if (h)
                                 manager.RestorePreviousPosition(h);
                             });
                           });
  hotkeyMgr.RegisterHotkey(
      HotkeyManager::HK_ARRANGE_ALL, MOD_CONTROL | MOD_ALT, '3', [&](int) {
        checkGameMode([&]() { manager.ArrangeAllWindowsNoOverlap(); });
      });
  hotkeyMgr.RegisterHotkey(HotkeyManager::HK_SAFE_CLOSE,
                           MOD_CONTROL | MOD_SHIFT, 'D', [&](int) {
                             checkGameMode([&]() {
                               HWND h = GetForegroundWindow();
                               if (h)
                                 manager.SafeCloseWindow(h);
                             });
                           });
  hotkeyMgr.RegisterHotkey(HotkeyManager::HK_TRANSPARENCY,
                           MOD_CONTROL | MOD_SHIFT, 'T', [&](int) {
                             checkGameMode([&]() {
                               HWND h = GetForegroundWindow();
                               if (h)
                                 manager.ToggleTransparency(h);
                             });
                           });

  // Dinámicos (Layouts y Apps)
  auto registerDynamicHotkeys = [&]() {
    for (int id = HotkeyManager::HK_LAYOUT_BASE;
         id < HotkeyManager::HK_APP_BASE; ++id)
      hotkeyMgr.UnregisterHotkey(id);
    for (int id = HotkeyManager::HK_APP_BASE;
         id < HotkeyManager::HK_APP_BASE + 100; ++id)
      hotkeyMgr.UnregisterHotkey(id);

    const auto &layouts = manager.GetLayouts();
    for (size_t i = 0; i < layouts.size(); ++i) {
      if (layouts[i].hotkey != 0) {
        hotkeyMgr.RegisterHotkey(HotkeyManager::HK_LAYOUT_BASE + (int)i,
                                 MOD_CONTROL | MOD_ALT, layouts[i].hotkey,
                                 [&, i](int) {
                                   checkGameMode([&]() {
                                     HWND h = GetForegroundWindow();
                                     if (h) {
                                       manager.PlaySoundEffect(600, 100);
                                       manager.ApplyLayout(h, (int)i);
                                     }
                                   });
                                 });
      }
    }

    const auto &apps = manager.GetAppShortcuts();
    for (size_t i = 0; i < apps.size(); ++i) {
      if (apps[i].hotkey != 0) {
        hotkeyMgr.RegisterHotkey(HotkeyManager::HK_APP_BASE + (int)i,
                                 apps[i].modifier, apps[i].hotkey, [&, i](int) {
                                   checkGameMode([&]() {
                                     manager.ExecuteAppShortcutByIndex((int)i);
                                   });
                                 });
      }
    }
  };

  registerDynamicHotkeys();
  manager.RestoreSession();

  const UINT WM_USER_RELOAD_HOTKEYS = WM_USER + 101;
  MSG msg = {0};
  while (GetMessage(&msg, NULL, 0, 0) != 0) {
    if (msg.message == WM_HOTKEY) {
      hotkeyMgr.ProcessHotkey((int)msg.wParam);
    } else if (msg.message == WM_USER_RELOAD_HOTKEYS) {
      registerDynamicHotkeys();
    } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  if (hThread) {
    TerminateThread(hThread, 0);
    CloseHandle(hThread);
  }
  Gdiplus::GdiplusShutdown(gdiplusToken);
}

int main(int argc, char *argv[]) {
  bool isWorker = false;
  for (int i = 0; i < argc; ++i) {
    if (std::string(argv[i]) == "--worker") {
      isWorker = true;
      break;
    }
  }

  if (isWorker) {
    RunWorker();
  } else {
    HANDLE hMutex =
        CreateMutexA(NULL, TRUE, "Local\\WinVen_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      if (hMutex)
        CloseHandle(hMutex);
      return 0;
    }

    char szPath[MAX_PATH];
    GetModuleFileNameA(NULL, szPath, MAX_PATH);

    while (true) {
      STARTUPINFOA si = {sizeof(si)};
      PROCESS_INFORMATION pi;
      std::string cmd = "\"" + std::string(szPath) + "\" --worker";
      std::vector<char> cmdBuf(cmd.begin(), cmd.end());
      cmdBuf.push_back('\0');

      if (CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE,
                         CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
      }
      Sleep(2000);
    }

    if (hMutex) {
      ReleaseMutex(hMutex);
      CloseHandle(hMutex);
    }
  }
  return 0;
}
