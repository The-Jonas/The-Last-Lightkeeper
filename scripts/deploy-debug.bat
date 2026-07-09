@echo off
REM ======================================================================
REM DEBUG distribution build for "A Luz do Farol".
REM Same as deploy.bat, but compiles the DEBUG variant (mingw32-make
REM debug-dist: -ggdb -O0 -DDEBUG, console window with DEBUG logs) and
REM flags it: a DEBUG-BUILD.txt marker inside the bundle and a _DEBUG tag
REM in the .zip name (e.g. A-Luz-do-Farol_DEBUG_2026-06-30.zip).
REM
REM Use this only for diagnostics; ship the normal deploy.bat build to
REM players. Just double-click, or run from a terminal.
REM Requires MinGW (mingw32-make) on PATH.
REM ======================================================================

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0deploy.ps1" -Debug %*
set "RC=%ERRORLEVEL%"

echo.
pause
exit /b %RC%
