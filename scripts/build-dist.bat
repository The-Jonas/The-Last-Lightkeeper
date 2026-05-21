@echo off
REM Builds an optimized executable and fills .\dist with JOGO.exe + SDL DLLs + Recursos
REM Requires: MinGW/GCC on PATH as mingw32-make (same as usual project build).

cd /d "%~dp0\.."

where mingw32-make >nul 2>&1
if errorlevel 1 (
  echo erro: mingw32-make nao encontrado no PATH.
  exit /b 1
)

echo Executando mingw32-make dist ...
mingw32-make dist
if errorlevel 1 exit /b 1

echo.
echo Abrindo pasta dist...
start "" "%~dp0..\dist"
