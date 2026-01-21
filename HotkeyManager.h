#ifndef HOTKEY_MANAGER_H
#define HOTKEY_MANAGER_H

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <windows.h>

/**
 * @brief Gestor centralizado de hotkeys para WinVen
 *
 * Características:
 * - IDs organizados por categoría (sin magic numbers)
 * - Callbacks type-safe
 * - Registro/desregistro automático
 * - Validación de conflictos
 * - Conversión string <-> hotkey ("Ctrl+Shift+A")
 */
class HotkeyManager {
public:
  // IDs organizados por categoría
  enum HotkeyID {
    // Navigation (100-119)
    HK_NAV_LEFT = 100,
    HK_NAV_RIGHT = 101,
    HK_NAV_UP = 102,
    HK_NAV_DOWN = 103,

    // Window Management (120-139)
    HK_CYCLE_25 = 120,
    HK_RESTORE_POS = 121,
    HK_ARRANGE_ALL = 122,
    HK_SAFE_CLOSE = 123,
    HK_TRANSPARENCY = 124,

    // System (140-159)
    HK_OPEN_CONFIG = 140,
    HK_GAME_MODE = 141,

    // Layouts (200-299) - Dinámico
    HK_LAYOUT_BASE = 200,

    // Apps (300-399) - Dinámico
    HK_APP_BASE = 300
  };

  // Callback type
  using HotkeyCallback = std::function<void(int id)>;

  // Constructor/Destructor
  HotkeyManager();
  ~HotkeyManager();

  // Configuración
  void SetMessageWindow(HWND hwnd);

  // Registro
  bool RegisterHotkey(int id, UINT modifiers, UINT vk, HotkeyCallback callback);
  bool RegisterHotkey(int id, const std::string &hotkeyString,
                      HotkeyCallback callback);
  bool UnregisterHotkey(int id);
  void UnregisterAll();

  // Procesamiento
  void ProcessHotkey(int id);

  // Estado
  bool IsRegistered(int id) const;
  std::vector<int> GetRegisteredIds() const;
  std::string GetHotkeyString(int id) const;

  // Utilidades
  static std::string ModifiersToString(UINT modifiers);
  static std::string VkToString(UINT vk);
  static bool ParseHotkeyString(const std::string &str, UINT &modifiers,
                                UINT &vk);

  // Validación
  bool HasConflict(UINT modifiers, UINT vk) const;

private:
  struct HotkeyInfo {
    int id;
    UINT modifiers;
    UINT vk;
    HotkeyCallback callback;
    bool registered;
  };

  HWND messageWindow;
  std::map<int, HotkeyInfo> hotkeys;

  // Helpers
  std::string GetHotkeyDescription(int id) const;
};

#endif // HOTKEY_MANAGER_H
