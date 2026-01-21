#include "Logger.h"
#include <ctime>
#include <iomanip>
#include <sstream>

// Inicialización de variables estáticas
bool WinVenLogger::enabled = true;
WinVenLogger::Level WinVenLogger::minLevel = WinVenLogger::L_INFO;
std::string WinVenLogger::logPath = "winven.log";
CRITICAL_SECTION WinVenLogger::cs;

void WinVenLogger::Initialize() {
  static bool initialized = false;
  if (!initialized) {
    InitializeCriticalSection(&cs);
    initialized = true;
  }
}

void WinVenLogger::SetEnabled(bool enable) { enabled = enable; }

void WinVenLogger::SetMinLevel(Level level) { minLevel = level; }

void WinVenLogger::SetLogFile(const std::string &path) { logPath = path; }

std::string WinVenLogger::GetTimestamp() {
  SYSTEMTIME st;
  GetLocalTime(&st);

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << st.wYear << "-" << std::setw(2)
      << st.wMonth << "-" << std::setw(2) << st.wDay << " " << std::setw(2)
      << st.wHour << ":" << std::setw(2) << st.wMinute << ":" << std::setw(2)
      << st.wSecond << "." << std::setw(3) << st.wMilliseconds;

  return oss.str();
}

std::string WinVenLogger::LevelToString(Level level) {
  switch (level) {
  case L_DEBUG:
    return "DEBUG";
  case L_INFO:
    return "INFO ";
  case L_WARNING:
    return "WARN ";
  case L_ERROR:
    return "ERROR";
  default:
    return "?????";
  }
}

void WinVenLogger::WriteToFile(const std::string &message) {
  std::ofstream file(logPath, std::ios::app);
  if (file.is_open()) {
    file << message << std::endl;
    file.close();
  }
}

void WinVenLogger::Log(Level level, const std::string &message) {
  if (!enabled || level < minLevel) {
    return;
  }

  Initialize();

  EnterCriticalSection(&cs);

  try {
    std::ostringstream oss;
    oss << GetTimestamp() << " [" << LevelToString(level) << "] " << message;
    WriteToFile(oss.str());

// También output a OutputDebugString para debugging en Visual Studio
#ifdef _DEBUG
    OutputDebugStringA((oss.str() + "\n").c_str());
#endif
  } catch (...) {
    // Silenciar errores de logging para no crashear la app
  }

  LeaveCriticalSection(&cs);
}

void WinVenLogger::Debug(const std::string &message) { Log(L_DEBUG, message); }

void WinVenLogger::Info(const std::string &message) { Log(L_INFO, message); }

void WinVenLogger::Warning(const std::string &message) {
  Log(L_WARNING, message);
}

void WinVenLogger::Error(const std::string &message) { Log(L_ERROR, message); }

void WinVenLogger::Flush() {
  // En esta implementación simple, cada log escribe inmediatamente
  // No hay buffer que hacer flush
}

std::string WinVenLogger::GetLogPath() { return logPath; }
