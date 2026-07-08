@echo off
REM ======================================================================
REM Full distribution build for "A Luz do Farol".
REM Builds a self-contained, statically-linked JOGO.exe, bundles the SDL
REM DLLs + Recursos + config + readme into .\dist, and produces a
REM date-stamped .zip ready to send (e.g. A-Luz-do-Farol_2026-06-30.zip).
REM
REM Just double-click this file, or run it from a terminal.
REM Requires MinGW (mingw32-make) on PATH.
REM ======================================================================

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy.ps1" %*
set "RC=%ERRORLEVEL%"

echo.
pause
exit /b %RC%
