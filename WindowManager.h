#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <dwmapi.h>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

#pragma comment(lib, "dwmapi.lib")

// Estructura para definir una posición de ventana personalizada
struct WindowLayout {
  std::string name;
  float x;      // Posición X como porcentaje (0.0 - 1.0)
  float y;      // Posición Y como porcentaje (0.0 - 1.0)
  float width;  // Ancho como porcentaje (0.0 - 1.0)
  float height; // Altura como porcentaje (0.0 - 1.0)
  int hotkey;   // Código del hotkey (opcional)

  WindowLayout(const std::string &n = "", float x_ = 0.0f, float y_ = 0.0f,
               float w_ = 1.0f, float h_ = 1.0f, int hk = 0)
      : name(n), x(x_), y(y_), width(w_), height(h_), hotkey(hk) {}
};

// Estructura para abrir una aplicación con un atajo
struct AppShortcut {
  std::string name;
  std::string path;
  int hotkey;
  int modifier;

  AppShortcut(const std::string &n = "", const std::string &p = "", int hk = 0,
              int mod = (MOD_CONTROL | MOD_ALT))
      : name(n), path(p), hotkey(hk), modifier(mod) {}
};

// Estructura para apps encontradas en el sistema
struct DiscoveryApp {
  std::string name;
  std::string path;
};

// Estructura para guardar la posición anterior de una ventana
struct WindowState {
  RECT rect;
  bool isMaximized;
  bool isMinimized;
  DWORD startTime;
};

class WindowManager {
private:
  std::vector<WindowLayout> layouts;
  std::vector<AppShortcut> appShortcuts;
  std::vector<std::string> excludedApps;
  std::map<int, int> hotkeyToLayoutIndex;
  std::map<int, int> hotkeyToAppIndex;
  std::map<HWND, WindowState> previousStates; // Guardar estados anteriores
  std::map<HWND, int> windowCycleIndex;       // Índice de ciclo por ventana
  std::string configFile;
  int margin = 6;            // Margen entre ventanas
  int currentCycleIndex = 0; // Índice para navegación circular

  // Configuración General
  bool soundsEnabled = true;
  bool animationsEnabled = true;
  bool trayIconEnabled = true;
  int animationSpeed = 12;
  int transparencyLevel = 180; // Default 180
  volatile bool isGameMode = false;
  bool loggingEnabled = false; // Desactivado por defecto por petición
  bool autoStartEnabled = false;
  HWND gameModeIndicatorHwnd = NULL;

  MONITORINFO GetMonInfo(HWND hwnd);

  // Las 25 posiciones predefinidas (se inicializan en el constructor)
  std::vector<WindowLayout> positions25;

public:
  WindowManager(const std::string &configPath = "window_layouts.cfg");
  ~WindowManager();

  // Gestión de layouts
  void AddLayout(const WindowLayout &layout);
  void RemoveLayout(int index);
  WindowLayout &GetLayout(int index);
  int GetLayoutCount() const { return layouts.size(); }

  // Gestión de apps
  void AddAppShortcut(const AppShortcut &app);
  void ExecuteAppShortcut(int hotkey);
  void ExecuteAppShortcutByIndex(int index);
  void RemoveAppShortcut(int index);

  // Descubrimiento de apps
  std::vector<DiscoveryApp> DiscoverSystemApps();

  // Aplicar layouts
  void ApplyLayout(HWND hwnd, int layoutIndex);
  void ApplyLayoutByHotkey(HWND hwnd, int hotkey);

  // Funcionalidades avanzadas
  void CycleLayout(HWND hwnd, bool forward);
  void TileAllWindows();
  void MoveActiveWindow(HWND hwnd, int direction);
  void SwitchWindowFocus(bool forward);
  void ShowMissionControl();

  // ===== NUEVAS FUNCIONES SOLICITADAS =====
  // Game Mode
  void ToggleGameMode();
  bool IsGameMode() const { return isGameMode; }
  void ShowGameModeIndicator();
  void HideGameModeIndicator();
  void CloseNonEssentialApps();

  // Ctrl + Alt + 1: Ciclar 25 posiciones predefinidas
  void CyclePosition25(HWND hwnd);
  void InitializePositions25(); // Inicializa las 25 posiciones

  // Ctrl + Alt + 2: Restaurar posición anterior
  void SaveCurrentState(HWND hwnd);
  void RestorePreviousPosition(HWND hwnd);

  // Ctrl + Alt + 3: Ordenar ventanas sin superposición
  void ArrangeAllWindowsNoOverlap();

  // Ctrl + Shift + D: Cerrar ventana de forma segura
  void SafeCloseWindow(HWND hwnd);

  // Novedades Estéticas y de Control
  void ToggleTransparency(HWND hwnd);
  void ToggleAlwaysOnTop(HWND hwnd);
  void SmoothMoveWindow(HWND hwnd, int targetX, int targetY, int targetW,
                        int targetH);
  void TileMasterStack();
  void MoveWindowToMonitor(HWND hwnd, bool next);
  void ResizeActiveWindow(HWND hwnd, int direction);
  void BringToFront(HWND hwnd);
  void SendToBack(HWND hwnd);

  // Persistencia y Exclusión
  void SaveSession();
  void RestoreSession();
  void AddToExclusionList(const std::string &processName);
  bool IsExcluded(HWND hwnd);

  // Configuración
  void SetMargin(int m) { margin = m; }
  int GetMargin() const { return margin; }
  void SetTransparencyLevel(int t) { transparencyLevel = t; }
  void SetSoundsEnabled(bool enabled) { soundsEnabled = enabled; }
  void SetAnimationsEnabled(bool enabled) { animationsEnabled = enabled; }
  void SetTrayIconEnabled(bool enabled) { trayIconEnabled = enabled; }
  void SetLoggingEnabled(bool enabled);
  void SetAutoStartEnabled(bool enabled);

  bool IsSoundsEnabled() const { return soundsEnabled; }
  bool IsAnimationsEnabled() const { return animationsEnabled; }
  bool IsTrayIconEnabled() const { return trayIconEnabled; }
  bool IsLoggingEnabled() const { return loggingEnabled; }
  bool IsAutoStartEnabled() const { return autoStartEnabled; }
  int GetTransparencyLevel() const { return transparencyLevel; }

  void PlaySoundEffect(int frequency, int duration);

  bool SaveConfig();
  bool LoadConfig();

  // Registro de hotkeys
  bool RegisterAllHotkeys(HWND messageWindow = NULL);
  void UnregisterAllHotkeys(HWND messageWindow = NULL);

  // Getters para UI
  const std::vector<WindowLayout> &GetLayouts() const { return layouts; }
  const std::vector<AppShortcut> &GetAppShortcuts() const {
    return appShortcuts;
  }

  // Utilidades
  std::vector<HWND> GetAllWindows();
  std::string GetWindowTitle(HWND hwnd);
  void CenterWindow(HWND hwnd);
  void CreateDefaultLayouts();
  void CreateDefaultAppShortcuts();
};

#endif // WINDOW_MANAGER_H
