#include "ConfigManager.h"
#include <algorithm>
#include <windows.h>

ConfigManager::ConfigManager(const std::string &path) : configPath(path) {
  // Si no existe, crear configuración por defecto
  std::ifstream test(configPath);
  if (!test.good()) {
    CreateDefault();
  }
}

std::string ConfigManager::Trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

std::string ConfigManager::RemoveQuotes(const std::string &str) {
  std::string result = Trim(str);
  if (result.length() >= 2 && result[0] == '"' &&
      result[result.length() - 1] == '"') {
    return result.substr(1, result.length() - 2);
  }
  return result;
}

bool ConfigManager::ParseLine(const std::string &line, std::string &key,
                              std::string &value) {
  size_t colonPos = line.find(':');
  if (colonPos == std::string::npos)
    return false;

  key = RemoveQuotes(line.substr(0, colonPos));

  std::string rawValue = line.substr(colonPos + 1);
  size_t commaPos = rawValue.find(',');
  if (commaPos != std::string::npos) {
    rawValue = rawValue.substr(0, commaPos);
  }

  value = Trim(rawValue);
  return true;
}

bool ConfigManager::Load() {
  std::ifstream file(configPath);
  if (!file.is_open())
    return false;

  values.clear();
  std::string line;

  while (std::getline(file, line)) {
    std::string key, value;
    if (ParseLine(line, key, value)) {
      values[key] = value;
    }
  }

  file.close();
  return true;
}

bool ConfigManager::Save() {
  std::ofstream file(configPath);
  if (!file.is_open())
    return false;

  file << "{\n";

  size_t count = 0;
  for (auto it = values.begin(); it != values.end(); ++it) {
    file << "  \"" << it->first << "\": ";

    // Detectar tipo y formatear apropiadamente
    std::string val = it->second;
    if (val == "true" || val == "false") {
      file << val;
    } else if (val.find_first_not_of("0123456789-") == std::string::npos) {
      file << val;
    } else {
      file << "\"" << val << "\"";
    }

    if (++count < values.size()) {
      file << ",";
    }
    file << "\n";
  }

  file << "}\n";
  file.close();
  return true;
}

std::string ConfigManager::GetString(const std::string &key,
                                     const std::string &defaultValue) {
  if (values.find(key) != values.end()) {
    return RemoveQuotes(values[key]);
  }
  return defaultValue;
}

int ConfigManager::GetInt(const std::string &key, int defaultValue) {
  if (values.find(key) != values.end()) {
    try {
      return std::stoi(values[key]);
    } catch (...) {
      return defaultValue;
    }
  }
  return defaultValue;
}

bool ConfigManager::GetBool(const std::string &key, bool defaultValue) {
  if (values.find(key) != values.end()) {
    std::string val = Trim(values[key]);
    return (val == "true" || val == "1");
  }
  return defaultValue;
}

void ConfigManager::SetString(const std::string &key,
                              const std::string &value) {
  values[key] = value;
}

void ConfigManager::SetInt(const std::string &key, int value) {
  values[key] = std::to_string(value);
}

void ConfigManager::SetBool(const std::string &key, bool value) {
  values[key] = value ? "true" : "false";
}

bool ConfigManager::Exists(const std::string &key) {
  return values.find(key) != values.end();
}

void ConfigManager::CreateDefault() {
  // Configuración por defecto según PROMT.md
  SetBool("sounds_enabled", true);
  SetBool("animations_enabled", true);
  SetBool("tray_icon_enabled", true);
  SetBool("auto_start", false);

  SetInt("margin", 6);
  SetInt("transparency_level", 180);
  SetInt("animation_speed", 12);

  SetString("config_version", "1.0");
  SetString("hotkeys.config_panel", "Ctrl+Alt+0");
  SetString("hotkeys.game_mode", "Ctrl+Alt+J");

  Save();
}
