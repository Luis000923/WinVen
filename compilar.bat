@echo off
echo ========================================
echo   Compilando WinVen...
echo ========================================
echo.

cd /d "%~dp0"

echo [1/2] Compilando recursos...
windres winven.rc -O coff -o winven.res
if %ERRORLEVEL% NEQ 0 (
    echo [WARN] No se pudo compilar recursos, continuando sin icono...
    set MAIN_RES=
) else (
    echo [OK] Recursos compilados
    set MAIN_RES=winven.res
)

echo [2/2] Compilando gestor_ven.exe...
g++ -o app/gestor_ven.exe gestor_ven.cpp ConfigGUI.cpp WindowManager.cpp ConfigManager.cpp Logger.cpp HotkeyManager.cpp %MAIN_RES% -mwindows -static -static-libgcc -static-libstdc++ -lole32 -loleaut32 -luuid -lshlwapi -ldwmapi -lgdiplus -lcomctl32 -luxtheme
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Fallo al compilar gestor_ven.exe
    pause
    exit /b 1
)
echo [OK] gestor_ven.exe compilado

echo.
echo ========================================
echo   Compilacion completada!
echo   Ejecutable en: app/gestor_ven.exe
echo ========================================
pause
