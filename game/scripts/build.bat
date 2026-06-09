@echo off
setlocal EnableExtensions
cd /d "%~dp0\.."
set "ROOT=%CD%"

rem CMake + vcpkg manifest (vcpkg.json). No LunarG SDK required.

if defined VCPKG_ROOT if not exist "%VCPKG_ROOT%\vcpkg.exe" set "VCPKG_ROOT="
if not defined VCPKG_ROOT (
  for /f "usebackq delims=" %%R in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\setup_vcpkg.ps1"`) do set "VCPKG_ROOT=%%R"
)
if not defined VCPKG_ROOT (
  echo [build.bat] error: vcpkg bootstrap failed
  exit /b 1
)
set "VCPKG_TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake"
echo [build.bat] VCPKG_ROOT=%VCPKG_ROOT%

"%VCPKG_ROOT%\vcpkg.exe" install --x-manifest-root="%ROOT%" --triplet x64-windows
if errorlevel 1 exit /b 1

set "VS_CMAKE="
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe 2^>nul`) do (
  if not defined VS_CMAKE set "VS_CMAKE=%%I"
)
if not defined VS_CMAKE (
  where cmake >nul 2>&1
  if not errorlevel 1 set "VS_CMAKE=cmake"
)
if not defined VS_CMAKE (
  echo [build.bat] error: cmake not found — install Visual Studio 2022 with C++ CMake tools
  exit /b 1
)
echo [build.bat] cmake=%VS_CMAKE%

if not exist "%ROOT%\build\external\enet\include\enet\enet.h" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%\scripts\fetch_externals.ps1"
)

set "BUILD_DIR=%ROOT%\build\cmake-vk"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

"%VS_CMAKE%" -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE="%VCPKG_TOOLCHAIN%" ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
if errorlevel 1 exit /b 1

"%VS_CMAKE%" --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

if exist "%ROOT%\bin\Release\zh_game.exe" (
  echo [build.bat] ok: "%ROOT%\bin\Release\zh_game.exe"
) else (
  echo [build.bat] error: exe not found
  exit /b 1
)

if defined GITHUB_ACTIONS (
  echo [build.bat] skipping gen_compile_commands in CI
  exit /b 0
)

call "%~dp0gen_compile_commands.bat"
if errorlevel 1 exit /b 1
exit /b 0
