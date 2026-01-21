#include "HotkeyManager.h"
#include "Logger.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>


HotkeyManager::HotkeyManager() : messageWindow(NULL) {
  LOG_INFO("HotkeyManager inicializado");
}

HotkeyManager::~HotkeyManager() {
  UnregisterAll();
  LOG_INFO("HotkeyManager destruido");
}

void HotkeyManager::SetMessageWindow(HWND hwnd) { messageWindow = hwnd; }

bool HotkeyManager::RegisterHotkey(int id, UINT modifiers, UINT vk,
                                   HotkeyCallback callback) {
  // Validar que no estÃ© ya registrado
  if (hotkeys.find(id) != hotkeys.end()) {
    LOG_WARNING(std::string("Hotkey ID ") + std::to_string(id) +
                " ya esta registrado");
    return false;
  }

  // Verificar conflictos
  if (HasConflict(modifiers, vk)) {
    LOG_WARNING(std::string("Conflicto detectado para hotkey: ") +
                ModifiersToString(modifiers) + "+" + VkToString(vk));
    return false;
  }

  // Registrar con Windows
  if (!RegisterHotKey(messageWindow, id, modifiers, vk)) {
    LOG_ERROR(std::string("Fallo al registrar hotkey ID ") +
              std::to_string(id) + ": " + ModifiersToString(modifiers) + "+" +
              VkToString(vk));
    return false;
  }

  // Guardar informaciÃ³n
  HotkeyInfo info;
  info.id = id;
  info.modifiers = modifiers;
  info.vk = vk;
  info.callback = callback;
  info.registered = true;

  hotkeys[id] = info;

  LOG_INFO(std::string("Hotkey registrado: ID=") + std::to_string(id) + " " +
           ModifiersToString(modifiers) + "+" + VkToString(vk));
  return true;
}

bool HotkeyManager::RegisterHotkey(int id, const std::string &hotkeyString,
                                   HotkeyCallback callback) {
  UINT modifiers, vk;
  if (!ParseHotkeyString(hotkeyString, modifiers, vk)) {
    LOG_ERROR(std::string("Formato de hotkey invalido: ") + hotkeyString);
    return false;
  }

  return RegisterHotkey(id, modifiers, vk, callback);
}

bool HotkeyManager::UnregisterHotkey(int id) {
  auto it = hotkeys.find(id);
  if (it == hotkeys.end()) {
    return false;
  }

  if (it->second.registered) {
    UnregisterHotKey(messageWindow, id);
    LOG_INFO(std::string("Hotkey desregistrado: ID=") + std::to_string(id));
  }

  hotkeys.erase(it);
  return true;
}

void HotkeyManager::UnregisterAll() {
  for (auto &pair : hotkeys) {
    if (pair.second.registered) {
      UnregisterHotKey(messageWindow, pair.first);
    }
  }
  hotkeys.clear();
  LOG_INFO("Todos los hotkeys desregistrados");
}

void HotkeyManager::ProcessHotkey(int id) {
  auto it = hotkeys.find(id);
  if (it == hotkeys.end()) {
    LOG_WARNING(std::string("Hotkey ID ") + std::to_string(id) +
                " no encontrado");
    return;
  }

  if (it->second.callback) {
    try {
      it->second.callback(id);
    } catch (const std::exception &e) {
      LOG_ERROR(std::string("Excepcion en callback de hotkey ID ") +
                std::to_string(id) + ": " + e.what());
    } catch (...) {
      LOG_ERROR(std::string("Excepcion desconocida en callback de hotkey ID ") +
                std::to_string(id));
    }
  }
}

bool HotkeyManager::IsRegistered(int id) const {
  return hotkeys.find(id) != hotkeys.end();
}

std::vector<int> HotkeyManager::GetRegisteredIds() const {
  std::vector<int> ids;
  for (const auto &pair : hotkeys) {
    ids.push_back(pair.first);
  }
  return ids;
}

std::string HotkeyManager::GetHotkeyString(int id) const {
  auto it = hotkeys.find(id);
  if (it == hotkeys.end()) {
    return "";
  }

  return ModifiersToString(it->second.modifiers) + "+" +
         VkToString(it->second.vk);
}

std::string HotkeyManager::ModifiersToString(UINT modifiers) {
  std::string result;

  if (modifiers & MOD_CONTROL) {
    if (!result.empty())
      result += "+";
    result += "Ctrl";
  }
  if (modifiers & MOD_ALT) {
    if (!result.empty())
      result += "+";
    result += "Alt";
  }
  if (modifiers & MOD_SHIFT) {
    if (!result.empty())
      result += "+";
    result += "Shift";
  }
  if (modifiers & MOD_WIN) {
    if (!result.empty())
      result += "+";
    result += "Win";
  }

  return result;
}

std::string HotkeyManager::VkToString(UINT vk) {
  // Letras A-Z
  if (vk >= 'A' && vk <= 'Z') {
    return std::string(1, (char)vk);
  }

  // NÃºmeros 0-9
  if (vk >= '0' && vk <= '9') {
    return std::string(1, (char)vk);
  }

  // Teclas especiales
  switch (vk) {
  case VK_LEFT:
    return "Left";
  case VK_RIGHT:
    return "Right";
  case VK_UP:
    return "Up";
  case VK_DOWN:
    return "Down";
  case VK_SPACE:
    return "Space";
  case VK_RETURN:
    return "Enter";
  case VK_ESCAPE:
    return "Esc";
  case VK_TAB:
    return "Tab";
  case VK_BACK:
    return "Backspace";
  case VK_DELETE:
    return "Delete";
  case VK_INSERT:
    return "Insert";
  case VK_HOME:
    return "Home";
  case VK_END:
    return "End";
  case VK_PRIOR:
    return "PageUp";
  case VK_NEXT:
    return "PageDown";
  default:
    return "VK_" + std::to_string(vk);
  }
}

bool HotkeyManager::ParseHotkeyString(const std::string &str, UINT &modifiers,
                                      UINT &vk) {
  modifiers = 0;
  vk = 0;

  std::istringstream iss(str);
  std::string token;
  std::vector<std::string> parts;

  while (std::getline(iss, token, '+')) {
    // Trim whitespace
    token.erase(0, token.find_first_not_of(" \t"));
    if (token.empty())
      continue;
    token.erase(token.find_last_not_of(" \t") + 1);
    parts.push_back(token);
  }

  if (parts.empty()) {
    return false;
  }

  // Ãšltimo token es la tecla, los demÃ¡s son modificadores
  for (size_t i = 0; i < parts.size() - 1; ++i) {
    std::string mod = parts[i];
    std::transform(mod.begin(), mod.end(), mod.begin(), ::tolower);

    if (mod == "ctrl" || mod == "control") {
      modifiers |= MOD_CONTROL;
    } else if (mod == "alt") {
      modifiers |= MOD_ALT;
    } else if (mod == "shift") {
      modifiers |= MOD_SHIFT;
    } else if (mod == "win" || mod == "windows") {
      modifiers |= MOD_WIN;
    } else if (mod == "altgr") {
      modifiers |= MOD_CONTROL | MOD_ALT;
    } else {
      return false; // Modificador desconocido
    }
  }

  // Parsear tecla
  std::string key = parts.back();

  // Letras
  if (key.length() == 1 && isalpha(key[0])) {
    vk = toupper(key[0]);
    return true;
  }

  // NÃºmeros
  if (key.length() == 1 && isdigit(key[0])) {
    vk = key[0];
    return true;
  }

  // Teclas especiales
  std::transform(key.begin(), key.end(), key.begin(), ::tolower);
  if (key == "left")
    vk = VK_LEFT;
  else if (key == "right")
    vk = VK_RIGHT;
  else if (key == "up")
    vk = VK_UP;
  else if (key == "down")
    vk = VK_DOWN;
  else if (key == "space")
    vk = VK_SPACE;
  else if (key == "enter")
    vk = VK_RETURN;
  else if (key == "esc" || key == "escape")
    vk = VK_ESCAPE;
  else if (key == "tab")
    vk = VK_TAB;
  else
    return false;

  return vk != 0;
}

bool HotkeyManager::HasConflict(UINT modifiers, UINT vk) const {
  for (const auto &pair : hotkeys) {
    if (pair.second.modifiers == modifiers && pair.second.vk == vk) {
      return true;
    }
  }
  return false;
}
