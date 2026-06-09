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

set "VS_CMAKE="
set "VS_NINJA="
set "VS_VCVARS="
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe 2^>nul`) do (
  if not defined VS_CMAKE set "VS_CMAKE=%%I"
)
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe 2^>nul`) do (
  if not defined VS_NINJA set "VS_NINJA=%%I"
)
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat 2^>nul`) do (
  if not defined VS_VCVARS set "VS_VCVARS=%%I"
)
if not defined VS_CMAKE (
  where cmake >nul 2>&1
  if not errorlevel 1 set "VS_CMAKE=cmake"
)
if not defined VS_VCVARS (
  echo [gen_compile_commands] error: Visual Studio vcvars64.bat not found
  exit /b 1
)
if not defined VS_CMAKE (
  echo [gen_compile_commands] error: cmake not found
  exit /b 1
)
if not defined VS_NINJA (
  echo [gen_compile_commands] error: ninja not found
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
