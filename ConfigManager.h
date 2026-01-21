#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

/**
 * @brief Gestor de configuración ligero con soporte JSON
 *
 * Implementación minimalista de parser JSON sin dependencias externas.
 * Soporta tipos básicos: string, int, bool, arrays
 */
class ConfigManager {
private:
  std::string configPath;
  std::map<std::string, std::string> values;

  // Helpers para parsing JSON simple
  std::string Trim(const std::string &str);
  std::string RemoveQuotes(const std::string &str);
  bool ParseLine(const std::string &line, std::string &key, std::string &value);

public:
  ConfigManager(const std::string &path = "config.json");

  // Cargar/Guardar
  bool Load();
  bool Save();

  // Getters con valores por defecto
  std::string GetString(const std::string &key,
                        const std::string &defaultValue = "");
  int GetInt(const std::string &key, int defaultValue = 0);
  bool GetBool(const std::string &key, bool defaultValue = false);

  // Setters
  void SetString(const std::string &key, const std::string &value);
  void SetInt(const std::string &key, int value);
  void SetBool(const std::string &key, bool value);

  // Utilidades
  bool Exists(const std::string &key);
  void CreateDefault();
  std::string GetPath() const { return configPath; }
};

#endif // CONFIG_MANAGER_H
