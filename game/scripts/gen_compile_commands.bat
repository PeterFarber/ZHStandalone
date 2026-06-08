@echo off
setlocal EnableExtensions

rem ---------------------------------------------------------------------------
rem Generate compile_commands.json for clangd / C++ IntelliSense (Cursor / VS Code).
rem Premake emits into build\. We copy to repo root so clangd finds it while
rem editing sources under game/src and game/include.
rem ---------------------------------------------------------------------------

cd /d "%~dp0\.."
set "ROOT=%CD%"

set "W64DEVKIT=%USERPROFILE%\Documents\CPP\w64devkit"
if exist "%W64DEVKIT%\bin\" (
  set "PATH=%W64DEVKIT%\bin;%PATH%"
) else if exist "%ROOT%\w64devkit\bin\" (
  set "PATH=%ROOT%\w64devkit\bin;%PATH%"
)

if not exist "%ROOT%\build\premake5.exe" (
  echo [gen_compile_commands] error: missing build\premake5.exe
  exit /b 1
)

pushd "%ROOT%\build" || exit /b 1
premake5.exe ecc
set "ECC=%ERRORLEVEL%"
popd
if not "%ECC%"=="0" exit /b 1

copy /Y "%ROOT%\build\compile_commands.json" "%ROOT%\compile_commands.json" >nul
echo [gen_compile_commands] ok: "%ROOT%\compile_commands.json"
exit /b 0
