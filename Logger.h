#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <string>
#include <windows.h>

/**
 * @brief Sistema de logging ligero para WinVen
 *
 * Características:
 * - Niveles de log (DEBUG, INFO, WARNING, ERROR)
 * - Timestamps automáticos
 * - Rotación de archivos (opcional)
 * - Thread-safe
 * - Bajo overhead
 */
class WinVenLogger {
public:
// Undefine conflicting macros from windows.h
#undef DEBUG
#undef INFO
#undef WARNING
#undef ERROR

  enum Level { L_DEBUG = 0, L_INFO = 1, L_WARNING = 2, L_ERROR = 3 };

  // Configuración
  static void SetEnabled(bool enable);
  static void SetMinLevel(Level level);
  static void SetLogFile(const std::string &path);

  // Logging
  static void Log(Level level, const std::string &message);
  static void Debug(const std::string &message);
  static void Info(const std::string &message);
  static void Warning(const std::string &message);
  static void Error(const std::string &message);

  // Utilidades
  static void Flush();
  static std::string GetLogPath();

private:
  static bool enabled;
  static Level minLevel;
  static std::string logPath;
  static CRITICAL_SECTION cs; // Thread safety

  // Helpers
  static std::string GetTimestamp();
  static std::string LevelToString(Level level);
  static void WriteToFile(const std::string &message);
  static void Initialize();
};

// Macros convenientes
#define LOG_DEBUG(msg) WinVenLogger::Debug(msg)
#define LOG_INFO(msg) WinVenLogger::Info(msg)
#define LOG_WARNING(msg) WinVenLogger::Warning(msg)
#define LOG_ERROR(msg) WinVenLogger::Error(msg)

#endif // LOGGER_H
