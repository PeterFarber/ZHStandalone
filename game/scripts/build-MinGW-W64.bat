@echo off
rem Regenerates Makefile (quickstart parity). Prefer game\scripts\build.bat for one-shot builds.
cd /d "%~dp0\.."
set "ROOT=%CD%"
cd /d "%ROOT%\build"
"%ROOT%\build\premake5.exe" gmake
cd /d "%ROOT%"
where mingw32-make >nul 2>nul && mingw32-make clean || make clean 2>nul
pause
