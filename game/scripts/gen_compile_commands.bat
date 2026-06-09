@echo off
setlocal EnableExtensions
cd /d "%~dp0\.."
set "ROOT=%CD%"

rem Ninja configure for clangd only (VS Release builds use build/cmake-vk/).

if defined VCPKG_ROOT if not exist "%VCPKG_ROOT%\vcpkg.exe" set "VCPKG_ROOT="
if not defined VCPKG_ROOT (
  for /f "usebackq delims=" %%R in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\setup_vcpkg.ps1"`) do set "VCPKG_ROOT=%%R"
)
if not defined VCPKG_ROOT (
  echo [gen_compile_commands] error: vcpkg bootstrap failed
  exit /b 1
)
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"

if not exist "%ROOT%\build\external\enet\include\enet\enet.h" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\fetch_externals.ps1"
)

set "VS_CMAKE=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "VS_NINJA=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "VS_VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VS_CMAKE%" (
  set "VS_CMAKE=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
  set "VS_NINJA=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
  set "VS_VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
if not exist "%VS_VCVARS%" (
  echo [gen_compile_commands] error: Visual Studio vcvars64.bat not found
  exit /b 1
)
call "%VS_VCVARS%" >nul

set "BUILD_DIR=%ROOT%\build\cmake-clangd"
"%VS_CMAKE%" -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%VS_NINJA%" ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
if errorlevel 1 exit /b 1

if not exist "%BUILD_DIR%\compile_commands.json" (
  echo [gen_compile_commands] error: missing compile_commands.json
  exit /b 1
)

copy /Y "%BUILD_DIR%\compile_commands.json" "%ROOT%\compile_commands.json" >nul
echo [gen_compile_commands] ok: "%ROOT%\compile_commands.json"
exit /b 0
