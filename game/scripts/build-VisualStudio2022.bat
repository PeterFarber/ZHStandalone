@echo off
cd /d "%~dp0\.."
set "ROOT=%CD%"
cd /d "%ROOT%\build"
"%ROOT%\build\premake5.exe" vs2022 || pause
