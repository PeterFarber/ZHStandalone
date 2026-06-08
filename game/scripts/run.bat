@echo off
setlocal

cd /d "%~dp0\.."
set "ROOT=%CD%"

set "EXE="
if exist "%ROOT%\bin\Release\zh_game.exe" set "EXE=%ROOT%\bin\Release\zh_game.exe"
if not defined EXE if exist "%ROOT%\bin\Debug\zh_game.exe" set "EXE=%ROOT%\bin\Debug\zh_game.exe"

if not defined EXE (
  echo [run.bat] zh_game.exe not found under bin\Release or bin\Debug
  echo [run.bat] run game\scripts\build.bat first
  exit /b 1
)

echo [run.bat] %EXE% %*

for %%p in ("%EXE%") do cd /d "%%~dp"

"%EXE%" %*
exit /b %ERRORLEVEL%
